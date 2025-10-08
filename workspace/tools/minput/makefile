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

TARGET = minput
COMPOSITOR_DIR = ../../system/compositor
CLIENT_LIB_DIR = $(COMPOSITOR_DIR)/client_lib
INCDIR = -I. -I../../lib/libcommon/ -I$(COMPOSITOR_DIR) -I$(CLIENT_LIB_DIR)
SOURCE = $(TARGET).c \
../../lib/libcommon/lang.c \
../../lib/libcommon/utils.c \
../../lib/libcommon/api.c ../../lib/libcommon/config.c \
../../lib/libcommon/scaler.c \
../../lib/libcommon/platform.c \
$(CLIENT_LIB_DIR)/client_lib.c

CC = $(CROSS_COMPILE)gcc
CFLAGS  += $(ARCH) -fomit-frame-pointer
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -std=gnu99
LDFLAGS	 += -lmsettings
ifeq ($(PLATFORM), tg5040)
CFLAGS += -DHAS_WIFIMG
LDFLAGS +=  -lwifimg -lwifid -lrt -luapi -lEGL -lGLESv2
endif

PRODUCT= build/$(PLATFORM)/$(TARGET).elf

all: #$(PREFIX_LOCAL)/include/msettings.h
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)
clean:
	rm -f $(PRODUCT)

# $(PREFIX_LOCAL)/include/msettings.h:
# 	cd ../../system/libmsettings && make