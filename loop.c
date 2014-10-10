#include <fcntl.h>
#include <linux/loop.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "debug.h"
#include "loop.h"


// Note: klibc demands the third ioctl arg to be a void*.

int logetfree(void)
{
	int fd = open("/dev/loop-control", O_RDWR);
	if (fd < 0) {
		WARNING("Failed to open '/dev/loop-control': %d\n", fd);
		return -1;
	}

	int devnr = ioctl(fd, LOOP_CTL_GET_FREE, NULL);
	if (devnr < 0) {
		WARNING("Failed to acquire free loop device: %d\n", devnr);
	} else {
		DEBUG("Got free loop device: %d\n", devnr);
	}

	close(fd);
	return devnr;
}

int losetup(const char *loop, const char *file)
{
	DEBUG("Setting up loop: '%s' via '%s'\n", file, loop);

	int filefd = open(file, O_RDONLY);
	if (filefd < 0) {
		ERROR("losetup: cannot open '%s': %d\n", file, filefd);
		return -1;
	}

	int loopfd = open(loop, O_RDONLY);
	if (loopfd < 0) {
		ERROR("losetup: cannot open '%s': %d\n", loop, loopfd);
		close(filefd);
		return -1;
	}

	int res = ioctl(loopfd, LOOP_SET_FD, (void *)(intptr_t)filefd);
	if (res < 0) {
		ERROR("Cannot setup loop device '%s': %d\n", loop, res);
	}

	close(loopfd);
	close(filefd);
	return res;
}
