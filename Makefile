CC ?= gcc
CFLAGS += -Wall -Wextra -Wno-unused-parameter -lole32 -Os -O3 -s \
	-fmerge-all-constants -fno-asynchronous-unwind-tables \
	-Wl,--gc-sections -Wl,--build-id=none

CHOST = $(shell $(CC) -dumpmachine)
EXTRA_CFLAGS = -fno-ident -fno-stack-protector -fomit-frame-pointer \
	-fno-unwind-tables -falign-functions=1 -falign-jumps=1 -falign-loops=1

ifneq (,$(findstring x86_64-w64-,$(CHOST)))
    EXTRA_CFLAGS += -Wl,-T,x86_64.ldscript
else ifneq (,$(findstring i686-w64-,$(CHOST)))
    EXTRA_CFLAGS += -Wl,-T,i386.ldscript
endif

all: mastervol

mastervol: 
	$(CC) mastervol.c -o mastervol $(CFLAGS)

small:
	$(CC) mastervol.c -o mastervol $(CFLAGS) $(EXTRA_CFLAGS)

clean:
	-@rm -f mastervol.exe

.PHONY: clean
.SILENT: clean