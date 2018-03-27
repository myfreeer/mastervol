CC ?= gcc
CFLAGS += -Wall -Wextra -lole32 -lwinmm -O3 -Os -s -nostartfiles \
	-fmerge-all-constants -fno-asynchronous-unwind-tables \
	-Wl,--gc-sections -Wl,--build-id=none

CHOST = $(shell $(CC) -dumpmachine)
EXTRA_CFLAGS = -fno-ident -fno-stack-protector -fomit-frame-pointer \
	-fno-unwind-tables -falign-functions=1 -falign-jumps=1 -falign-loops=1 \
	-ffunction-sections -fdata-sections  -fno-math-errno

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
	-@rm -f *.exe *.o

test: small
	./mastervol --help
	./mastervol 50
	./mastervol 55 -m
	./mastervol -u
	./mastervol -s 50
	./mastervol

%.exe: %.c
	$(CC) $< -o $@ $(CFLAGS) $(EXTRA_CFLAGS)

.PHONY: clean
.SILENT: clean