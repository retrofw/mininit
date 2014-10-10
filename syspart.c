#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <errno.h>
#include <stdlib.h>

#ifndef __KLIBC__
#include <sys/syscall.h>
#else
#include <sys/mount.h>
#endif /* __KLIBC__ */
#include <unistd.h>

#include "debug.h"
#include "mininit.h"


const char *mount_boot(void)
{
	return "/";
}

int open_dir_to_clean(void)
{
	return -1;
}

int switch_root(void)
{
	DEBUG("Pivoting root\n");
#ifndef __KLIBC__
	if (syscall(SYS_pivot_root, ".", "boot")) {
#else
	if (pivot_root(".", "boot") < 0) {
#endif /* __KLIBC__ */
		ERROR("Unable to pivot root: %d\n", errno);
		return -1;
	}

	return 0;
}
