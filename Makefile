# CFLAGS can be overridden, CFLAGS_NOCUST should be left untouched.
CFLAGS ?= -O2 -fomit-frame-pointer
CFLAGS_NOCUST = -std=c99 -Wall -Wextra -Wundef -Wold-style-definition
LDFLAGS = -s -static

BINARIES = mininit-initramfs mininit-syspart splashkill

M_OBJS = mininit.o loop.o
S_OBJS = splashkill.o
OBJS := $(M_OBJS) initramfs.o syspart.o $(S_OBJS)

.PHONY: all clean

all: $(BINARIES)

clean:
	rm -f $(OBJS) $(BINARIES)

mininit-initramfs: $(M_OBJS) initramfs.o
mininit-syspart: $(M_OBJS) syspart.o
splashkill: $(S_OBJS)

# Don't bother with fine-grained dependency tracking: just recompile everything
# on any header change.
$(OBJS): $(wildcard *.h)

$(OBJS): %.o: %.c
	$(CC) $(CFLAGS_NOCUST) $(CFLAGS) -o $@ -c $<

$(BINARIES):
	$(CC) $(CFLAGS_NOCUST) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.o,$^)
