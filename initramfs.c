#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef MS_MOVE
#define MS_MOVE 8192
#endif

#ifndef O_PATH
#define O_PATH 010000000
#endif

#include "debug.h"
#include "mininit.h"


#define BOOTFS_TYPE    "vfat"

static const char *boot_mount = "/boot";


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

const char *mount_boot(void)
{
	/* Create boot and root mount points.
	 * Failure is most likely fatal, but perhaps mkdir on a usable mount point
	 * could return something other than EEXIST when trying to recreate it. */
	create_mount_point(boot_mount);
	create_mount_point("/root");

	/* Process "boot" parameter (comma-separated list).
	 * Note that we specify 20 retries (2 seconds), just in case it is
	 * a hotplug device which takes some time to detect and initialize. */
	char *boot_dev = getenv("boot");
	if (boot_dev) {
		if (multi_mount(boot_dev, boot_mount, BOOTFS_TYPE, MS_RDONLY, 20)) {
			return NULL;
		}
	} else {
		ERROR("'boot' parameter not found\n");
		return NULL;
	}

	return boot_mount;
}

int open_dir_to_clean(void)
{
	int fd = open("/", O_PATH | O_DIRECTORY);
	if (fd < 0) {
		DEBUG("Failed to open '/' before switch: %d\n", errno);
	}
	return fd;
}

int switch_root(void)
{
	DEBUG("Switching root\n");

	/* Move the boot mount to inside the rootfs tree. */
	if (mount(boot_mount, "boot", NULL, MS_MOVE, NULL)) {
		ERROR("Unable to move the '%s' mount: %d\n", boot_mount, errno);
		return -1;
	}

	/* Do the root switch */
	if (mount(".", "/", NULL, MS_MOVE, NULL)) {
		ERROR("Unable to switch to the new root: %d\n", errno);
		return -1;
	}

	return 0;
}
