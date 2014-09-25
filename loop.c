#include <sys/ioctl.h>

#include <errno.h>
#include <string.h>

#include <linux/loop.h>

#include "loop.h"


int losetup(int loopfd, int filefd, const char *filename)
{
	struct loop_info64 lo;
	if (strlen(filename) + 1 > sizeof(lo.lo_file_name)) return ENAMETOOLONG;

	int ret = ioctl(loopfd, LOOP_SET_FD, filefd);
	if (ret < 0) return ret;

	memset(&lo, 0, sizeof(lo));
	strcpy((char *)lo.lo_file_name, filename);

	return ioctl(loopfd, LOOP_SET_STATUS64, &lo);
}
