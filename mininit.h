// Implemented by mininit.c:

/**
 * Creates a directory at the given path, suitable to be used as a mount point.
 * If the given path already exists, nothing happens.
 * Returns 0 on success and -1 on errors.
 */
int create_mount_point(const char *path);

// Implemented by backends:

/**
 * Makes sure the boot partition (the partition hosting the rootfs image)
 * is mounted.
 * Returns the mount point of the boot partition on success and NULL on errors.
 */
const char *mount_boot(void);

/**
 * Returns a file descriptor for the directory to be cleaned up before we exec.
 * If there is nothing to clean up, return -1.
 * If there was an error opening the directory, return -1 as well.
 */
int open_dir_to_clean(void);

/**
 * Make the current directory the root directory and preserve the boot partition
 * in the "boot" subdirectory of the current directory.
 * Returns 0 on success and -1 on errors.
 */
int switch_root(void);
