#ifndef LOOP_H
#define LOOP_H

/**
 * Find or allocate a free loop device.
 * Returns the minor block device number (0-255), or -1 on errors.
 */
int logetfree(void);

/**
 * Set the given file as a backing file for the given loop device.
 * Returns 0 on success and -1 on errors.
 */
int losetup(const char *loop, const char *file);

#endif // LOOP_H
