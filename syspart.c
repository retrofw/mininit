#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <errno.h>
#include <stdlib.h>
#include <sys/syscall.h>
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
	if (syscall(SYS_pivot_root, ".", "boot")) {
		ERROR("Unable to pivot root: %d\n", errno);
		return -1;
	}

	return 0;
}
