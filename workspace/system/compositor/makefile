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

TARGET = compositor
INCDIR = -I.
SOURCE = $(TARGET).c
LIBSOURCE = libfb_$(TARGET).c
# CFLAGS = 
# CFLAGS_GNU = -Wall -O2 -g -std=gnu99
LDFLAGS = -lrt -lm
CC = $(CROSS_COMPILE)gcc
CFLAGS  += $(ARCH) -fomit-frame-pointer -Wall -O2 -g -std=c99
# CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -std=gnu99
# LDFLAGS	 += -lmsettings -lgametimedb -lsqlite3

PRODUCT= build/$(PLATFORM)/$(TARGET).elf

LIBPRODUCT= build/$(PLATFORM)/libfb_$(TARGET).so

.PHONY: all clean

all:
	apt-get install libdrm-dev
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)

clean:
	rm -f $(PRODUCT)

# # LD_PRELOAD拦截库
# libfb_compositor:
# 	@echo "编译framebuffer拦截库..."
# 	mkdir -p build/$(PLATFORM)
# 	$(CC) -shared -fPIC $(SOURCE) $(CFLAGS) -o $(LIBPRODUCT) -ldl $(LDFLAGS)
# $(PREFIX_LOCAL)/include/gametimedb.h:
# 	cd ../../system/libgametimedb && make