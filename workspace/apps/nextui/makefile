
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
###########################################################

include ../../lib/libcommon/makefile.env
SDL?=SDL

###########################################################

TARGET = nextui

INCDIR = -I./include -I$(PREFIX_LOCAL)/include

SOURCE = $(TARGET).c datastructures.c browser.c uimanager.c launcher.c async.c

CC = $(CROSS_COMPILE)gcc
CFLAGS  += $(ARCH) -fomit-frame-pointer
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -std=gnu99

LDFLAGS += -L$(PREFIX_LOCAL)/lib -lcommon -lmsettings

ifeq ($(PLATFORM), tg5040)
CFLAGS += -DHAS_WIFIMG
LDFLAGS +=  -lwifimg -lwifid
endif

PRODUCT= build/$(PLATFORM)/$(TARGET).elf

all: 
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(PRODUCT)