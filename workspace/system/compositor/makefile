ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif

ifeq (,$(PLATFORM))
	$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif

ifeq (,$(CROSS_COMPILE))
	$(error missing CROSS_COMPILE for this toolchain)
endif
# 编译器
CC = $(CROSS_COMPILE)gcc

# 目录
BUILD_DIR       = build/$(PLATFORM)
APP_DIR         = apps
LIB_DIR         = client_lib
COMMON_DIR      = ../../lib/libcommon
MSETTINGS_LIB_DIR   = $(PREFIX_LOCAL)/lib
MSETTINGS_INC_DIR   = $(PREFIX_LOCAL)/include

# 头文件包含路径
INCDIRS         = -I. -I/usr/include -I$(LIB_DIR) -I$(COMMON_DIR) -I$(MSETTINGS_INC_DIR)

# 编译标志
CFLAGS          = -Wall -O2 -g -std=gnu99 -DUSE_SDL2 $(INCDIRS) -DPLATFORM=\"$(PLATFORM)\" `sdl2-config --cflags`

# --- 链接库定义 ---
# MODIFIED: 合成器的新链接选项，只包含必要的库
COMPOSITOR_LDFLAGS = -lrt -lpthread `sdl2-config --libs` -lGLESv2 -lEGL

# 具备完整UI功能的程序所需的链接选项
GFX_LDFLAGS = -L$(MSETTINGS_LIB_DIR) -lmsettings -lwifid -lwifimg -lsamplerate -lrt -lpthread `sdl2-config --libs` -lSDL2_ttf -lm -lSDL2_image -lGLESv2 -ldl -lbtmg -lglib-2.0 -lgio-2.0 -lshared-mainloop -lbluetooth-internal -lasound -ljson-c -luapi

# --- 源文件依赖定义 ---
UI_APP_DEPS_SRC = \
	$(COMMON_DIR)/api.c \
	$(COMMON_DIR)/platform.c \
	$(COMMON_DIR)/config.c \
	$(COMMON_DIR)/utils.c \
	$(COMMON_DIR)/lang.c \
	$(COMMON_DIR)/scaler.c

# 源文件
SRC_COMPOSITOR      = compositor.c
SRC_APP_A           = $(APP_DIR)/app_A.c
SRC_APP_B_OVERLAY   = $(APP_DIR)/app_B_overlay.c
SRC_CLIENT_LIB      = $(LIB_DIR)/client_lib.c
SRC_APP_C_EXCLUSIVE = $(APP_DIR)/app_C_exclusive.c

# 目标文件
PROD_COMPOSITOR     = $(BUILD_DIR)/compositor.elf
# PROD_APP_A          = $(BUILD_DIR)/app_A.elf
PROD_APP_B_OVERLAY  = $(BUILD_DIR)/app_B_overlay.elf
OBJ_CLIENT_LIB      = $(BUILD_DIR)/client_lib.o
PROD_APP_C_EXCLUSIVE = $(BUILD_DIR)/app_C_exclusive.elf

TARGETS = $(PROD_COMPOSITOR) $(PROD_APP_B_OVERLAY) $(PROD_APP_C_EXCLUSIVE)

###########################################################
# 3. 编译规则 (Build Rules)
###########################################################

.PHONY: all clean

all: $(TARGETS)
	@echo "All targets built successfully."

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 规则：编译客户端库
$(OBJ_CLIENT_LIB): $(SRC_CLIENT_LIB) | $(BUILD_DIR)
	@echo "  CC (lib)  $(LIB_DIR)/client_lib.c"
	$(CC) $(CFLAGS) -c $(SRC_CLIENT_LIB) -o $@

# MODIFIED: 规则：编译合成器 (不再依赖 UI_APP_DEPS_SRC)
$(PROD_COMPOSITOR): $(SRC_COMPOSITOR) | $(BUILD_DIR)
	@echo "  LINK (compositor) $@"
	$(CC) $(CFLAGS) $(SRC_COMPOSITOR) -o $@ $(COMPOSITOR_LDFLAGS)

# # 规则：编译客户端A (假设存在)
# $(PROD_APP_A): $(SRC_APP_A) $(OBJ_CLIENT_LIB) $(UI_APP_DEPS_SRC) | $(BUILD_DIR)
# 	@echo "  LINK (app A) $@"
# 	$(CC) $(CFLAGS) $(SRC_APP_A) $(OBJ_CLIENT_LIB) $(UI_APP_DEPS_SRC) -o $@ $(GFX_LDFLAGS)

# 规则：编译客户端B (使用完整UI依赖)
$(PROD_APP_B_OVERLAY): $(SRC_APP_B_OVERLAY) $(OBJ_CLIENT_LIB) $(UI_APP_DEPS_SRC) | $(BUILD_DIR)
	@echo "  LINK (app B) $@"
	$(CC) $(CFLAGS) $(SRC_APP_B_OVERLAY) $(OBJ_CLIENT_LIB) $(UI_APP_DEPS_SRC) -o $@ $(GFX_LDFLAGS)

# 规则：编译客户端C (使用完整UI依赖)
$(PROD_APP_C_EXCLUSIVE): $(SRC_APP_C_EXCLUSIVE) $(OBJ_CLIENT_LIB) $(UI_APP_DEPS_SRC) | $(BUILD_DIR)
	@echo "  LINK (app C - exclusive) $@"
	$(CC) $(CFLAGS) $(SRC_APP_C_EXCLUSIVE) $(OBJ_CLIENT_LIB) $(UI_APP_DEPS_SRC) -o $@ $(GFX_LDFLAGS)

clean:
	@echo "Cleaning up..."
	rm -rf build