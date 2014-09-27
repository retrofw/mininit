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


#define BOOTFS_TYPE    "vfat"

#define ROOTFS_TYPE    "squashfs"
#define ROOTFS_CURRENT "/boot/rootfs." ROOTFS_TYPE
#define ROOTFS_BACKUP  ROOTFS_CURRENT ".bak"
#define ROOTFS_UPDATE  "/boot/update_r.bin"


static int multi_mount(
		char *source,
		const char *target,
		const char *type,
		unsigned long flags,
		int retries)
{
	for (int try = 0; try < retries; usleep(100000), try++) {
		for (char c = ',', *s = source; c == ','; *s++ = c) {
			char *t;
			for (t = s; *s != ',' && *s != '\0'; s++);
			c = *s;
			*s = '\0';

			if (!mount(t, target, type, flags, NULL)) {
				INFO("%s mounted on %s\n", t, target);
				*s = c;
				return 0;
			}
		}
	}

	ERROR("Cannot mount %s on %s\n", source, target);
	return -1;
}


void perform_updates(bool is_backup)
{
	bool update_modules = !access("/boot/update_m.bin", R_OK);
	bool update_rootfs = !access("/boot/update_r.bin", R_OK);
	if (!update_modules && !update_rootfs) return;

	DEBUG("Remounting '/boot' read-write\n");
	if (mount("/boot", "/boot", NULL, MS_REMOUNT | MS_NOATIME, NULL)) {
		ERROR("Unable to remount '/boot' read-write.\n");
		return;
	}

	if (update_modules) {
		DEBUG("Modules update found!\n");
		rename("/boot/modules.squashfs", "/boot/modules.squashfs.bak");
		rename("/boot/modules.squashfs.sha1", "/boot/modules.squashfs.bak.sha1");
		rename("/boot/update_m.bin", "/boot/modules.squashfs");
		rename("/boot/update_m.bin.sha1", "/boot/modules.squashfs.sha1");
	}

	if (update_rootfs) {
		DEBUG("RootFS update found!\n");

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

	DEBUG("Remounting '/boot' read-only\n");
	if (mount("/boot", "/boot", NULL, MS_REMOUNT | MS_RDONLY, NULL)) {
		ERROR("Unable to remount '/boot' read-only.\n");
	}
}


int main(int argc, char **argv, char **envp)
{
	INFO("\n\n\nOpenDingux min-init 1.1.0 "
				"by Ignacio Garcia Perez <iggarpe@gmail.com> "
				"and Paul Cercueil <paul@crapouillou.net>\n");

	/* Mount devtmpfs to get a full set of device nodes. */
	DEBUG("Mounting /dev\n");
	if (mount("devtmpfs", "/dev", "devtmpfs", 0, NULL)) {
		INFO("Couldn't mount devtmpfs on /dev: %d\n", errno);
		/* If there are sufficient static device nodes in the initramfs,
		 * we can boot without devtmpfs. */
	}

	/* Look for "rootfs_bak" parameter. */
	bool is_backup = false;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "rootfs_bak")) {
			is_backup = true;
			break;
		}
	}

	/* Process "boot" parameter (comma-separated list).
	 * Note that we specify 20 retries (2 seconds), just in case it is
	 * a hotplug device which takes some time to detect and initialize. */
	char *boot = getenv("boot");
	if (boot) {
		if (multi_mount(boot, "/boot", BOOTFS_TYPE, MS_RDONLY, 20)) {
			return -1;
		}
	} else {
		ERROR("'boot' parameter not found.\n");
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
	const char *rootfs =
		  is_backup && !access(ROOTFS_BACKUP, F_OK)
		? ROOTFS_BACKUP
		: ROOTFS_CURRENT;
	losetup(loop_dev, rootfs);

	/* Mount the loop device that was just set up. */
	if (mount(loop_dev, "/root", ROOTFS_TYPE, MS_RDONLY, NULL)) {
		ERROR("Failed to mount the rootfs image.\n");
		return -1;
	}

	/* Move the /boot mountpoint so that it is visible
	 * on the new filesystem tree */
	DEBUG("Moving '/boot' mountpoint\n");
	if (mount("/boot", "/root/boot", NULL, MS_MOVE, NULL)) {
		ERROR("Unable to move the '/boot' mountpoint.\n");
		return -1;
	}

	/* Now let's switch to the new root */
	DEBUG("Switching root\n");

	if (chdir("/root") < 0) {
		ERROR("Unable to change to '/root' directory.\n");
		return -1;
	}

	/* Re-open the console device at the new location
	 * (must already exist). */
	int fd = open("/root/dev/console", O_RDWR, 0);
	if (fd < 0) {
		ERROR("Unable to re-open console.\n");
		return -1;
	}

	if ((dup2(fd, 0) != 0)
			|| (dup2(fd, 1) != 1)
			|| (dup2(fd, 2) != 2)) {
		ERROR("Unable to duplicate console handles.\n");
		return -1;
	}
	if (fd > 2) close(fd);

	/* Keep the old root open until the chroot is done */
	fd = open("/", O_RDONLY, 0);

	/* Do the root switch */
	if (mount(".", "/", NULL, MS_MOVE, NULL)) {
		ERROR("Unable to switch to the new root.\n");
		close(fd);
		return -1;
	}

	if ((chroot(".") < 0) || (chdir("/") < 0)) {
		ERROR("'chroot' failed.\n");
		close(fd);
		return -1;
	}

	/* Release the old root */
	close(fd);

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
			ERROR("Unable to find the 'init' executable.\n");
			return -1;
		}
		if (!access(inits[i], X_OK)) {
			DEBUG("Found 'init' executable: %s\n", inits[i]);
			argv[0] = (char *)inits[i];
			break;
		}
	}

	/* Execute the 'init' executable */
	execvpe(argv[0], argv, envp);
	ERROR("exec or init failed.\n");
	return 0;
}
