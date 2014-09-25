#include <sys/ioctl.h>

#include <string.h>

#include <linux/loop.h>

#include "loop.h"


int losetup(int loopfd, int filefd, const char *filename)
{
	int ret = ioctl(loopfd, LOOP_SET_FD, filefd);
	if (ret < 0) return ret;

	struct loop_info64 lo;
	memset(&lo, 0, sizeof(lo));
	strncpy((char *)lo.lo_file_name, filename, LO_NAME_SIZE - 1);

	return ioctl(loopfd, LOOP_SET_STATUS64, &lo);
}
