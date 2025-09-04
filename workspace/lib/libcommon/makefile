ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif
ifeq (,$(PREFIX))
$(error missing PREFIX for this toolchain)
endif

include ./makefile.env
# 定义目标库名称
TARGET=common

# 定义源文件、头文件和目标文件
SOURCES = $(wildcard *.c)
HEADERS = $(wildcard *.h)
OBJECTS = $(SOURCES:.c=.o)

# 定义伪目标
.PHONY: build clean

# 定义编译器
CC = $(CROSS_COMPILE)gcc

# -------------------【关键修正】-------------------
#
# 定义编译标志 CFLAGS
# -I.                  让源文件能找到同目录下的其他头文件 (例如 common.h)
# -I$(PREFIX_LOCAL)/include  让源文件能找到其他模块安装的头文件 (例如 msettings.h)
#
CFLAGS += -std=gnu99 -DPLATFORM=\"$(PLATFORM)\" -Werror -fpic -I. -I../system/libmsettings -I$(PREFIX_LOCAL)/include

# 定义链接标志
LDFLAGS = -s -lsamplerate -lbtmg -lglib-2.0 -lgio-2.0 -lshared-mainloop -lbluetooth-internal -lasound -ljson-c
# ----------------------------------------------------

# build 目标：编译、链接并“安装”到本地目录
build: lib$(TARGET).so
	@echo "--- Installing to $(PREFIX_LOCAL) ---"
	@mkdir -p "$(PREFIX_LOCAL)/include"
	@mkdir -p "$(PREFIX_LOCAL)/lib"
	@cp -f $(HEADERS) "$(PREFIX_LOCAL)/include"
	@cp -f "lib$(TARGET).so" "$(PREFIX_LOCAL)/lib"

# 链接规则：从所有 .o 文件生成 libcommon.so
lib$(TARGET).so: $(OBJECTS)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

# 编译规则：从 .c 文件生成 .o 文件
# 这里会使用上面定义好的 CFLAGS
%.o: %.c
	$(CC) -c $(CFLAGS) $< -o $@

# clean 目标：清理生成的文件和本地“安装”的文件
clean:
	@echo "--- Cleaning build files ---"
	@rm -f $(OBJECTS)
	@rm -f "lib$(TARGET).so"
	@echo "--- Cleaning local installation ---"
	@rm -f $(addprefix $(PREFIX_LOCAL)/include/, $(notdir $(HEADERS)))
	@rm -f $(PREFIX_LOCAL)/lib/lib$(TARGET).so