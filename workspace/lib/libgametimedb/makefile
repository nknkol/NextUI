###########################################################

ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
	$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

ifeq (,$(CROSS_COMPILE))
	$(error missing CROSS_COMPILE for this toolchain)
endif

ifeq (,$(PREFIX))
	$(error missing PREFIX for this toolchain)
endif

###########################################################

include ../libcommon/makefile.env
SDL?=SDL

###########################################################

TARGET=gametimedb

.PHONY: build
.PHONY: clean

CC = $(CROSS_COMPILE)gcc

SYSROOT     := $(shell $(CC) --print-sysroot)
COMPOSITOR_DIR = ../../system/compositor
CLIENT_LIB_DIR = $(COMPOSITOR_DIR)/client_lib
INCDIR = -I. -I../libcommon/ -I$(COMPOSITOR_DIR) -I$(CLIENT_LIB_DIR)
SOURCE = $(TARGET).c ../libcommon/utils.c ../libcommon/api.c ../libcommon/platform.c

CFLAGS  += $(ARCH) -fomit-frame-pointer
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -std=gnu99
LDFLAGS += -s -lsqlite3

PRODUCT= build/$(PLATFORM)/lib$(TARGET).so

build: 
	mkdir -p build/$(PLATFORM)
	$(CC) -c -Werror -fpic $(SOURCE) -Wl,--no-as-needed $(LDFLAGS) $(CFLAGS)
	$(CC) -shared -o $(PRODUCT) "$(TARGET).o" "utils.o" $(LDFLAGS)
	cp "$(TARGET).h" "$(PREFIX_LOCAL)/include"
	cp $(PRODUCT) "$(PREFIX_LOCAL)/lib"

clean:
	rm -f *.o
	rm -f $(PRODUCT)
	rm -f $(PREFIX_LOCAL)/include/$(TARGET).h
	rm -f $(PREFIX_LOCAL)/lib/lib$(TARGET).so