#keymon

ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = keymon
# 添加新的源文件
SRCS = keymon.c bt_volume_sync.c

# 使用 pkg-config 获取 GLib/GIO 的编译和链接标志
PKGCFG = $(shell pkg-config --cflags --libs glib-2.0 gio-2.0)

CC = $(CROSS_COMPILE)gcc
# 在原有的 CFLAGS 基础上，追加新的库依赖
CFLAGS	= -Os -lmsettings -lpthread -lrt -ldl -Wl,--gc-sections -s $(PKGCFG)
CFLAGS  += -I. -I../../lib/libcommon/ -I$(PREFIX_LOCAL)/include -L$(PREFIX_LOCAL)/lib -DPLATFORM=\"$(UNION_PLATFORM)\"

all:
	# 修改编译命令以包含所有源文件
	$(CC) $(SRCS) -o $(TARGET).elf $(CFLAGS)

clean:
	rm -rf $(TARGET).elf