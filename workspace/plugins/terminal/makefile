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

TARGET = terminal
INCDIR = -I. -I../../lib/libcommon/ -I$(PREFIX_LOCAL)/include
# 插件只编译自己的源文件
SOURCE = $(TARGET).c

CC = $(CROSS_COMPILE)gcc
# 为共享库添加 -fPIC
CFLAGS  += $(ARCH) -fomit-frame-pointer -fPIC -DHAS_WIFIMG
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -std=gnu99
# 添加 -shared 并保留必要的库链接
LDFLAGS	 += -shared -lmsettings -lwifimg -lwifid -ltsm -lutil

# 将输出目标从 .elf 修改为 .so
PRODUCT= build/$(PLATFORM)/$(TARGET).so

all:
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)
clean:
	rm -f $(PRODUCT)
