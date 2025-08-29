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

TARGET = ledcontrol
INCDIR = -I. -I../../lib/libcommon/
SOURCE = $(TARGET).c 

CC = $(CROSS_COMPILE)gcc
CFLAGS  += $(ARCH) -fomit-frame-pointer -DHAS_WIFIMG -fPIC
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -std=gnu99
LDFLAGS	 += -lmsettings -shared -lwifimg -lwifid -lcommon

PRODUCT= build/$(PLATFORM)/$(TARGET).so

all: 
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)
clean:
	rm -f $(PRODUCT)
