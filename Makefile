CFLAGS = -std=c99 -O2 -fomit-frame-pointer
CFLAGS += -Wall -Wextra -Wundef -Wold-style-definition
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

$(BINARIES):
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(filter %.o,$^)
