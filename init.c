#define _BSD_SOURCE

#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#ifndef MS_MOVE
#define MS_MOVE 8192
#endif

#include "debug.h"
#include "loop.h"


#define ROOTFS_TYPE    "squashfs"
#define ROOTFS_CURRENT "/boot/rootfs." ROOTFS_TYPE
#define ROOTFS_BACKUP  ROOTFS_CURRENT ".bak"
#define ROOTFS_UPDATE  "/boot/update_r.bin"


/* Split the passed buffer as a list of parameters. */
static int __mkparam (char *buf, char **paramv, int maxparam, const char delim)
{
	paramv[0] = strtok(buf, &delim);
	if (!paramv[0])
		return 0;

	int paramc;
	for (paramc=1; ; paramc++) {
		paramv[paramc] = strtok(NULL, &delim);
		if (!paramv[paramc]) break;
	}

	return paramc;
}

static int __read_text_file (const char *fn, char *buf, size_t len)
{
	int fd = open(fn, O_RDONLY);
	if (fd < 0) {
		ERROR("Unable to open \'%s\'.\n", fn);
		return fd;
	}

	int r = read(fd, buf, len-1);
	close(fd);
	if (r < 0) {
		ERROR("Unable to read \'%s\'.\n", fn);
		return r;
	}

	/* Skip the last \n */
	if (r && (buf[r-1] == '\n'))
		buf[r-1] = '\0';

	buf[r] = '\0';
	return 0;
}

static int __mount (
		const char *source,
		const char *target,
		const char *type,
		unsigned long flags)
{
	int nb;
	char cbuf[4096];
	char * tokens[64];

	if (type || (flags & MS_MOVE))
		return mount(source, target, type, flags, NULL);

	/* The filesystem is unknown.
	 * We will try each filesystem supported by the kernel. */
	if (__read_text_file("/proc/filesystems", cbuf, sizeof(cbuf)))
		return -1;

	nb = __mkparam(cbuf, tokens, sizeof(tokens)/sizeof(tokens[0]), '\n');

	while (nb--) {
		/* note: the possible filesystems all start with a
		 * tabulation in that file, except ubifs */
		if (!strncmp(tokens[nb], "nodev\tubifs", 11))
			tokens[nb] += 5;
		else if (*tokens[nb] != '\t')
			continue;

		/* skip the tabulation */
		tokens[nb]++;

		if (!mount(source, target, tokens[nb], flags, NULL))
			return 0;
	}

	DEBUG("Failed attempt to mount %s on %s\n", source, target);
	return -1;
}

static int __multi_mount (
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

			if (!__mount(t, target, type, flags)) {
				INFO("%s mounted on %s\n", t, target);
				*s = c;
				return 0;
			}
		}
	}

	ERROR("Cannot mount %s on %s\n", source, target);
	return -1;
}


int main(int argc, char **argv)
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

	DEBUG("Mounting /proc\n");
	if ( mount(NULL, "/proc", "proc", 0, NULL) ) {
		ERROR("Unable to mount /proc\n");
		return -1;
	}

	DEBUG("Reading kernel command line\n");
	char cbuf[4096];
	if (__read_text_file("/proc/cmdline", cbuf, sizeof(cbuf)))
		return -1;
	DEBUG("Command line read: %s\n", cbuf);

	/* paramv[0] and paramv[paramc] are reserved */
	char * paramv[64];
	int paramc = 1 + __mkparam(cbuf, paramv+1, sizeof(paramv)/sizeof(paramv[0]) -2, ' ');

	/* Look for "rootfs_bak" parameter. */
	bool is_backup = false;
	for (int i=1; i<paramc; i++) {
		if (!strcmp(paramv[i], "rootfs_bak")) {
			is_backup = true;
			break;
		}
	}

	/* Process "boot" parameter
	 * (only one, allow comma-separated list).
	 * Note that we specify 20 retries (2 seconds), just in case it is
	 * a hotplug device which takes some time to detect and initialize. */
	bool boot = false;
	for (int i=1; i<paramc; i++) {
		if (!strncmp(paramv[i], "boot=", 5)) {
			if ( __multi_mount(paramv[i]+5, "/boot", NULL, 0, 20) )
				return -1;
			boot = true;
			break;
		}
	}
	if (!boot) {
		ERROR("\'boot\' parameter not found.\n");
		return -1;
	}

	/* Check for a modules fs update */
	if (!access("/boot/update_m.bin", R_OK | W_OK)) {
		DEBUG("Modules update found!\n");
		rename("/boot/modules.squashfs", "/boot/modules.squashfs.bak");
		rename("/boot/modules.squashfs.sha1", "/boot/modules.squashfs.bak.sha1");
		rename("/boot/update_m.bin", "/boot/modules.squashfs");
		rename("/boot/update_m.bin.sha1", "/boot/modules.squashfs.sha1");
	}

	/* Check for a rootfs update */
	if (!access("/boot/update_r.bin", R_OK | W_OK)) {
		DEBUG("RootFS update found!\n");

		/* If rootfs_bak was not passed, or the backup is not available,
		 * make a backup of the current rootfs before the update */
		if (!is_backup || access(ROOTFS_BACKUP, F_OK)) {
			rename(ROOTFS_CURRENT, ROOTFS_BACKUP);
			rename(ROOTFS_CURRENT ".sha1", ROOTFS_BACKUP ".sha1");
		}

		rename(ROOTFS_UPDATE, ROOTFS_CURRENT);
		rename(ROOTFS_UPDATE ".sha1", ROOTFS_CURRENT ".sha1");
		sync();
	}

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
	DEBUG("Moving \'/boot\' mountpoint\n");
	if ( mount("/boot", "/root/boot", NULL, MS_MOVE, NULL) ) {
		ERROR("Unable to move the \'/boot\' mountpoint.\n");
		return -1;
	}

	/* Remount /boot readonly */
	DEBUG("Remounting \'/root/boot\' read-only\n");
	if ( mount("/root/boot", "/root/boot", NULL,
					MS_REMOUNT | MS_RDONLY, NULL) ) {
		ERROR("Unable to remount \'/root/boot\' read-only.\n");
		return -1;
	}

	/* Now let's switch to the new root */
	DEBUG("Switching root\n");

	if (chdir("/root") < 0) {
		ERROR("Unable to change to \'/root\' directory.\n");
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

	/* Unmount the previously mounted /proc filesystem. */
	umount("/proc");

	/* Do the root switch */
	if ( mount(".", "/", NULL, MS_MOVE, NULL) ) {
		ERROR("Unable to switch to the new root.\n");
		close(fd);
		return -1;
	}

	if ((chroot(".") < 0) || (chdir("/") < 0)) {
		ERROR("\'chroot\' failed.\n");
		close(fd);
		return -1;
	}

	/* Release the old root */
	close(fd);

	/* Prepare paramv[0] which is the init program itself */
	char sbuf [256];
	int i;
	for (i=1; i<paramc; i++) {
		if (strncmp(paramv[i], "init=", 5))
			continue;
		strcpy(sbuf, paramv[i]+5);
		break;
	}

	/* If no 'init=' is found on the command line, we try to
	 * locate the init program. */
	if (i >= paramc) {
		const char *inits [] = {
			"/sbin/init",
			"/etc/init",
			"/bin/init",
			"/bin/sh",
			NULL,
		};
		for (i=0; inits[i] && access(inits[i], X_OK)<0; i++);
		if (!inits[i]) {
			ERROR("Unable to find the \'init\' executable.\n");
			return -1;
		}
		strcpy(sbuf, inits[i]);
	}

	paramv[0] = sbuf;

	/* Execute the 'init' executable */
	execv(paramv[0], paramv);
	ERROR("exec or init failed.\n");
	return 0;
}
