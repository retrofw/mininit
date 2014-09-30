#define _BSD_SOURCE
#define _GNU_SOURCE

#include <errno.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "debug.h"
#include "mininit.h"


const char *mount_boot()
{
	return "/";
}

int open_dir_to_clean()
{
	return -1;
}

int switch_root()
{
	DEBUG("Pivoting root\n");
	if (syscall(SYS_pivot_root, ".", "boot")) {
		ERROR("Unable to pivot root: %d\n", errno);
		return -1;
	}

	return 0;
}
