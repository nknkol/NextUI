# ###########################################################

# ifeq (,$(PLATFORM))
# PLATFORM=$(UNION_PLATFORM)
# endif

# ifeq (,$(PLATFORM))
# 	$(error please specify PLATFORM, eg. PLATFORM=trimui make)
# endif

# ifeq (,$(CROSS_COMPILE))
# 	$(error missing CROSS_COMPILE for this toolchain)
# endif

# ###########################################################

# include ../../lib/libcommon/makefile.env
# SDL?=SDL

# ###########################################################

# TARGET = nextui
# INCDIR = -I. -I../../lib/libcommon/
# SOURCE = $(TARGET).c ../../lib/libcommon/lang.c ../../lib/libcommon/scaler.c ../../lib/libcommon/utils.c ../../lib/libcommon/config.c ../../lib/libcommon/api.c ../../lib/libcommon/platform.c

# CC = $(CROSS_COMPILE)gcc
# CFLAGS  += $(ARCH) -fomit-frame-pointer
# CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -std=gnu99
# LDFLAGS	 += -lmsettings
# ifeq ($(PLATFORM), tg5040)
# CFLAGS += -DHAS_WIFIMG
# LDFLAGS +=  -lwifimg -lwifid
# endif

# PRODUCT= build/$(PLATFORM)/$(TARGET).elf

# all: #$(PREFIX_LOCAL)/include/msettings.h
# 	mkdir -p build/$(PLATFORM)
# 	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)
# clean:
# 	rm -f $(PRODUCT)

# # $(PREFIX_LOCAL)/include/msettings.h:
# # 	cd ../system/libmsettings && make

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

# 修改1: 只包含头文件目录，不包含源文件
INCDIR = -I. -I$(PREFIX_LOCAL)/include

# 修改2: 只编译 nextui.c，不包含 libcommon 的源文件
SOURCE = $(TARGET).c

CC = $(CROSS_COMPILE)gcc
CFLAGS  += $(ARCH) -fomit-frame-pointer
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -std=gnu99

# 修改3: 添加库搜索路径和链接 libcommon
LDFLAGS += -L$(PREFIX_LOCAL)/lib -lcommon -lmsettings

ifeq ($(PLATFORM), tg5040)
CFLAGS += -DHAS_WIFIMG
LDFLAGS +=  -lwifimg -lwifid
endif

PRODUCT= build/$(PLATFORM)/$(TARGET).elf

# 修改4: 确保 libcommon 先编译完成
all: #$(PREFIX_LOCAL)/lib/libcommon.so
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(LDFLAGS)

# # 确保 libcommon.so 存在
# $(PREFIX_LOCAL)/lib/libcommon.so:
# 	cd ../../lib/libcommon && make

clean:
	rm -f $(PRODUCT)