#define _BSD_SOURCE
#define _GNU_SOURCE

#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#ifndef MS_MOVE
#define MS_MOVE 8192
#endif

#include "debug.h"
#include "loop.h"
#include "mininit.h"


#define ROOTFS_TYPE    "squashfs"
#define ROOTFS_CURRENT "rootfs." ROOTFS_TYPE
#define ROOTFS_BACKUP  ROOTFS_CURRENT ".bak"
#define ROOTFS_UPDATE  "update_r.bin"


int create_mount_point(const char *path)
{
	if (mkdir(path, 0755)) {
		if (errno != EEXIST) {
			WARNING("Failed to create '%s' mount point: %d\n", path, errno);
			return -1;
		}
	}
	return 0;
}

void perform_updates(bool is_backup)
{
	bool update_modules = !access("update_m.bin", R_OK);
	bool update_rootfs = !access("update_r.bin", R_OK);
	if (!update_modules && !update_rootfs) return;

	DEBUG("Remounting boot device read-write\n");
	if (mount(NULL, ".", NULL, MS_REMOUNT | MS_NOATIME, NULL)) {
		ERROR("Unable to remount boot device read-write: %d\n", errno);
		return;
	}

	if (update_modules) {
		DEBUG("Modules update found\n");
		rename("modules.squashfs", "modules.squashfs.bak");
		rename("modules.squashfs.sha1", "modules.squashfs.bak.sha1");
		rename("update_m.bin", "modules.squashfs");
		rename("update_m.bin.sha1", "modules.squashfs.sha1");
	}

	if (update_rootfs) {
		DEBUG("RootFS update found\n");

		/* If rootfs_bak was not passed, or the backup is not available,
		 * make a backup of the current rootfs before the update */
		if (!is_backup || access(ROOTFS_BACKUP, F_OK)) {
			rename(ROOTFS_CURRENT, ROOTFS_BACKUP);
			rename(ROOTFS_CURRENT ".sha1", ROOTFS_BACKUP ".sha1");
		}

		rename(ROOTFS_UPDATE, ROOTFS_CURRENT);
		rename(ROOTFS_UPDATE ".sha1", ROOTFS_CURRENT ".sha1");
	}

	sync();

	DEBUG("Remounting boot device read-only\n");
	if (mount(NULL, ".", NULL, MS_REMOUNT | MS_RDONLY, NULL)) {
		ERROR("Unable to remount boot device read-only: %d\n", errno);
	}
}


FILE *logfile;

int main(int argc, char **argv, char **envp)
{
	logfile = stderr;

	/* Mount devtmpfs to get a full set of device nodes. */
	if (mount("devtmpfs", "/dev", "devtmpfs", 0, NULL)) {
		INFO("Couldn't mount devtmpfs on /dev: %d\n", errno);
		/* If there are sufficient static device nodes in the fs containing
		 * mininit, we can boot without devtmpfs, so don't give up yet. */
	}

	/* Write our log messages to the kernel log. */
	FILE *kmsg = fopen("/dev/kmsg", "w");
	if (kmsg) {
		setlinebuf(kmsg);
		logfile = kmsg;
	}
	INFO("OpenDingux mininit 1.1.0\n");
	if (!kmsg) {
		WARNING("Failed to open '/dev/kmsg': %d\n", errno);
	}

	/* Look for "rootfs_bak" parameter. */
	bool is_backup = false;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "rootfs_bak")) {
			is_backup = true;
			break;
		}
	}

	const char *boot_mount = mount_boot();
	if (!boot_mount) {
		return -1;
	}

	if (chdir(boot_mount)) {
		ERROR("Unable to change to '%s' directory: %d\n", boot_mount, errno);
		return -1;
	}

	perform_updates(is_backup);

	/* Get free loop device. */
	int devnr = logetfree();
	if (devnr < 0) {
		/* We're running early in the boot sequence, probably /dev/loop0
		 * is still available. */
		devnr = 0;
	}
	char loop_dev[9 + 10 + 1];
	sprintf(loop_dev, "/dev/loop%i", devnr);

	/* Set the rootfs as the backing file for the loop device. */
	const char *rootfs_img =
		  is_backup && !access(ROOTFS_BACKUP, F_OK)
		? ROOTFS_BACKUP
		: ROOTFS_CURRENT;
	losetup(loop_dev, rootfs_img);

	/* Mount the loop device that was just set up. */
	DEBUG("Loop-mounting '%s' on '/root'\n", rootfs_img);
	if (mount(loop_dev, "/root", ROOTFS_TYPE, MS_RDONLY, NULL)) {
		ERROR("Failed to mount the rootfs image: %d\n", errno);
		return -1;
	}
	INFO("%s mounted on /root\n", rootfs_img);

	/* Make the freshly mounted rootfs image the working directory. */
	if (chdir("/root")) {
		ERROR("Unable to change to '/root' directory: %d\n", errno);
		return -1;
	}

	/* Move the devtmpfs mount to inside the rootfs tree. */
	DEBUG("Moving '/dev' mount\n");
	if (mount("/dev", "dev", NULL, MS_MOVE, NULL)) {
		ERROR("Unable to move the '/dev' mount: %d\n", errno);
		return -1;
	}

	/* Re-open the console device at the new location. */
	int fd = open("dev/console", O_RDWR, 0);
	if (fd < 0) {
		ERROR("Unable to re-open console: %d\n", fd);
		return -1;
	}
	if (dup2(fd, 0) != 0 || dup2(fd, 1) != 1 || dup2(fd, 2) != 2) {
		ERROR("Unable to duplicate console handles\n");
		return -1;
	}
	if (fd > 2) close(fd);

	/* Open the old root while we can still access it. */
	fd = open_dir_to_clean();

	/* Now let's switch to the new root */
	if (switch_root()) {
		if (fd >= 0) close(fd);
		return -1;
	}

	/* Make the freshly switched root the root of this process. */
	if (chroot(".")) {
		ERROR("'chroot' to new root failed: %d\n", errno);
		if (fd >= 0) close(fd);
		return -1;
	}

	/* And make it the working directory as well. */
	if (chdir("/")) {
		ERROR("'chdir' to new root failed: %d\n", errno);
		if (fd >= 0) close(fd);
		return -1;
	}

	INFO("root switch done\n");

	/* Clean up the initramfs and then release it. */
	if (fd >= 0) {
		DEBUG("Removing initramfs contents\n");
		const char *executable = argv[0];
		while (*executable == '/') ++executable;
		if (unlinkat(fd, executable, 0)) {
			DEBUG("Failed to remove '%s' executable: %d\n", executable, errno);
		}
		if (unlinkat(fd, "dev/console", 0)) {
			DEBUG("Failed to remove '/dev/console': %d\n", errno);
		}
		if (unlinkat(fd, "dev", AT_REMOVEDIR)) {
			DEBUG("Failed to remove '/dev' directory: %d\n", errno);
		}
		if (unlinkat(fd, "boot", AT_REMOVEDIR)) {
			DEBUG("Failed to remove '/boot' mount point: %d\n", errno);
		}
		if (unlinkat(fd, "root", AT_REMOVEDIR)) {
			DEBUG("Failed to remove '/root' directory: %d\n", errno);
		}
		if (close(fd)) {
			DEBUG("Failed to close initramfs: %d\n", errno);
		}
	}

	/* Try to locate the init program. */
	const char *inits[] = {
		"/sbin/init",
		"/etc/init",
		"/bin/init",
		"/bin/sh",
		NULL,
	};
	for (int i = 0; ; i++) {
		if (!inits[i]) {
			ERROR("Unable to find the 'init' executable\n");
			return -1;
		}
		DEBUG("Checking for 'init' executable: %s\n", inits[i]);
		if (!access(inits[i], X_OK)) {
			argv[0] = (char *)inits[i];
			break;
		}
	}
	INFO("starting %s\n", argv[0]);

	if (kmsg) {
		logfile = stderr;
		fclose(kmsg);
	}

	/* Execute the 'init' executable */
	execvpe(argv[0], argv, envp);
	ERROR("Exec of 'init' failed: %d\n", errno);
	return -1;
}
