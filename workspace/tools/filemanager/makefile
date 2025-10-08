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

TARGET = file_manager
COMPOSITOR_DIR = ../../system/compositor
CLIENT_LIB_DIR = $(COMPOSITOR_DIR)/client_lib
# 引用公共头文件的路径，与 nextui 的 makefile 保持一致
INCDIR = -I. -I$(PREFIX_LOCAL)/include -I$(COMPOSITOR_DIR) -I$(CLIENT_LIB_DIR)

# SOURCE 变量现在只包含程序自身的源文件
SOURCE = $(TARGET).c keyboard.c $(CLIENT_LIB_DIR)/client_lib.c

CC = $(CROSS_COMPILE)gcc
CFLAGS  += $(ARCH) -fomit-frame-pointer
CFLAGS  += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\" -std=gnu99

# LDFLAGS 直接链接 -lcommon，并指定库路径
LDFLAGS += -L$(PREFIX_LOCAL)/lib -lcommon -lmsettings -lrt -luapi -lEGL -lGLESv2

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