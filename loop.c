#include <fcntl.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "debug.h"
#include "loop.h"


int losetup(const char *loop, const char *file)
{
	DEBUG("Setting up loop: '%s' via '%s'.\n", file, loop);

	int filefd = open(file, O_RDONLY);
	if (filefd < 0) {
		ERROR("losetup: cannot open '%s'.\n", file);
		return -1;
	}

	int loopfd = open(loop, O_RDONLY);
	if (loopfd < 0) {
		ERROR("losetup: cannot open '%s'.\n", loop);
		close(filefd);
		return -1;
	}

	int res = ioctl(loopfd, LOOP_SET_FD, filefd);
	if (res < 0) {
		ERROR("Cannot setup loop device '%s'.\n", loop);
	}

	close(loopfd);
	close(filefd);
	return res;
}
