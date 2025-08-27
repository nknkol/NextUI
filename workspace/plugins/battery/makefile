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

# 迁移: 目标名称更改为插件名
TARGET = battery
INCDIR = -I. -I../../lib/libcommon/

# 迁移: 插件只编译自身源码，libcommon中的代码由主程序提供
SOURCE = $(TARGET).c

CC = $(CROSS_COMPILE)gcc

# 迁移: 添加 -fPIC 以生成位置无关代码 (共享库必需)
CFLAGS  += $(ARCH) -fomit-frame-pointer -fPIC -DHAS_WIFIMG
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -std=gnu99

# 迁移: 添加 -shared 以生成共享库，保留原有库依赖
LDFLAGS	 += -shared -lmsettings -lbatmondb -lsqlite3 -lwifimg -lwifid

# 迁移: 输出文件扩展名改为 .so
PRODUCT= build/$(PLATFORM)/$(TARGET).so

all:
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)
clean:
	rm -f $(PRODUCT)