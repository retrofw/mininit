Mininit
=======

This program enables you to use a squashfs image as the root filesystem of an embedded Linux system. If you want to use a different file system instead of squashfs, change the `ROOTFS_TYPE` macro in `mininit.c`.

You need to specify a `boot=X` parameter on the kernel command line, where `X` is a comma-separated list of the possible device nodes for the filesystem on which the rootfs image is stored. A typical value is `boot=/dev/mmcblk0p1`, for the first partition of the first SD card.

The file system selected using the `boot=` parameter is expected to be a FAT file system with long file names (vfat). If you want to use a different file system, change the `BOOTFS_TYPE` macro in `mininit.c`.

Initramfs Layout
----------------

Min-init should be run as the `/init` process in initramfs. It can run without any other files present in the initramfs, but for debugging it is recommended to have a working `/dev/console`. An example `tree.txt` file for `CONFIG_INITRAMFS_SOURCE` would look like this:

```
dir     /dev                            0755    0 0
nod     /dev/console                    0644    0 0	c 5	1
file    /init           mininit         0755    0 0
```

Boot Partition Layout
---------------------

The following file names are used for the rootfs image on the boot partition:
```
rootfs.squashfs          : root file system
rootfs.squashfs.bak      : fallback root file system
update_r.bin             : replacement root file system
```

If a replacement rootfs image is present, the following rename sequence is performed:
> update_r.bin -> rootfs.squashfs -> rootfs.squashfs.bak -> (deleted)

For every image file in the list, there can be a text file with a `.sha1` suffix that contains the checksum of the image file. Note that Mininit doesn't perform checksum checks, but it does apply the same rename sequence to checksum files on updates, such that they are kept in sync with their respective image files.

Optionally, the boot partition can also hold images containing kernel modules:
```
modules.squashfs         : kernel modules file system
modules.squashfs.bak     : fallback kernel modules file system
update_m.bin             : replacement kernel modules file system
```
Mininit takes care of renaming these images on updates, similar to the rootfs update mechanism, but it doesn't mount the modules image.

RootFS Image Layout
-------------------

Inside the rootfs image, mount points are expected at `/dev` and `/boot`, where devtmpfs and the boot partition will end up respectively. The boot partition will be mounted read-only when control is transferred to the `/sbin/init` of the rootfs.

Building
--------

To compile, pass your cross compiler to Make via the CC variable, like this:

> $ make CC=mipsel-gcw0-linux-uclibc-gcc

If you want additional debug logging, see `LOG_LEVEL` in `debug.h`.

Credits
-------

Developed for OpenDingux (http://www.treewalker.org/opendingux/) by Paul Cercueil <paul@crapouillou.net>.

Based on mininit from Ignacio Garcia Perez (http://code.google.com/p/dingoo-linux/).
