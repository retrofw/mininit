CFLAGS ?= -std=c99 -Wall -O2 -fomit-frame-pointer
LDFLAGS = -s -static

BINARIES = mininit splashkill

M_OBJS = mininit.o loop.o
S_OBJS = splashkill.o

.PHONY: all clean

all: $(BINARIES)

clean:
	rm -f $(M_OBJS) $(S_OBJS) $(BINARIES)

mininit: $(M_OBJS)
splashkill: $(S_OBJS)
