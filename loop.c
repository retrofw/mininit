#include <linux/loop.h>
#include <sys/ioctl.h>

#include "loop.h"


int losetup(int loopfd, int filefd)
{
	return ioctl(loopfd, LOOP_SET_FD, filefd);
}
