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

TARGET = clock
INCDIR = -I. -I../../lib/libcommon/
SOURCE = $(TARGET).c

CC = $(CROSS_COMPILE)gcc
# 修改: 添加 -fPIC 用于生成位置无关代码
CFLAGS  += $(ARCH) -fomit-frame-pointer -fPIC
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -std=gnu99

# 修改: 链接 libcommon
LDFLAGS	 += -L$(PREFIX_LOCAL)/lib -lcommon -lmsettings

CFLAGS += -DHAS_WIFIMG
LDFLAGS +=  -lwifimg -lwifid


# 修改: 目标文件为 .so
PRODUCT= build/$(PLATFORM)/$(TARGET).so

all:
	mkdir -p build/$(PLATFORM)
	# 修改: 添加 -shared 来创建共享库
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS) -shared
clean:
	rm -f $(PRODUCT)