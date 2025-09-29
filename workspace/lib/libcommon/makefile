ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif
ifeq (,$(PREFIX))
$(error missing PREFIX for this toolchain)
endif

include ./makefile.env
TARGET=common

BUILD_DIR = build/$(PLATFORM)
PRODUCT = $(BUILD_DIR)/lib$(TARGET).so

SOURCES = $(wildcard *.c)
HEADERS = $(wildcard *.h)

OBJECTS = $(addprefix $(BUILD_DIR)/, $(SOURCES:.c=.o))


.PHONY: build clean

CC = $(CROSS_COMPILE)gcc

CFLAGS += -std=gnu99 -DPLATFORM=\"$(PLATFORM)\" -Werror -fpic -I. -I$(PREFIX_LOCAL)/include -I../../system/compositor/client_lib -I../../system/compositor

# 定义链接标志
LDFLAGS = -s -lsamplerate -lbtmg -lglib-2.0 -lgio-2.0 -lshared-mainloop -lbluetooth-internal -lasound -ljson-c
# ----------------------------------------------------

build: $(PRODUCT)
	@echo "--- Installing to $(PREFIX_LOCAL) ---"
	@mkdir -p "$(PREFIX_LOCAL)/include"
	@mkdir -p "$(PREFIX_LOCAL)/lib"
	@cp -f $(HEADERS) "$(PREFIX_LOCAL)/include"
	# 【修改】从新的 PRODUCT 路径拷贝
	@cp -f "$(PRODUCT)" "$(PREFIX_LOCAL)/lib"

# 【修改】链接规则：从所有 .o 文件生成到 PRODUCT 路径
$(PRODUCT): $(OBJECTS)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: %.c
	# 在编译前确保输出目录存在
	@mkdir -p $(@D)
	$(CC) -c $(CFLAGS) $< -o $@

clean:
	@echo "--- Cleaning build files ---"
	# 直接删除整个 build 目录更简单高效
	@rm -rf build
	@echo "--- Cleaning local installation ---"
	@rm -f $(addprefix $(PREFIX_LOCAL)/include/, $(notdir $(HEADERS)))
	@rm -f $(PREFIX_LOCAL)/lib/lib$(TARGET).so