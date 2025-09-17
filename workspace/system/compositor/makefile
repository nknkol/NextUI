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
INCDIRS         = -I. -I$(LIB_DIR) -I$(COMMON_DIR) -I$(MSETTINGS_INC_DIR)

# 编译标志
CFLAGS          = -Wall -O2 -g -std=gnu99 -DUSE_SDL2 $(INCDIRS) -DPLATFORM=\"$(PLATFORM)\" `sdl2-config --cflags`

# --- 链接库定义 ---
# 核心图形库链接选项
GFX_LDFLAGS = -L$(MSETTINGS_LIB_DIR) -lmsettings -lrt -lpthread `sdl2-config --libs` -lSDL2_ttf -lm -ldl -lbtmg -lglib-2.0 -lgio-2.0 -lshared-mainloop -lbluetooth-internal -lasound -ljson-c
# 客户端应用的极简链接选项
CLIENT_LDFLAGS = -lrt -lpthread 

# --- 源文件依赖定义 ---
# 合成器所需的最少公共源文件
COMPOSITOR_DEPS_SRC = \
	$(COMMON_DIR)/api.c \
	$(COMMON_DIR)/platform.c \
	$(COMMON_DIR)/config.c \
	$(COMMON_DIR)/utils.c \
	$(COMMON_DIR)/scaler.c

# 源文件
SRC_COMPOSITOR      = compositor.c
SRC_APP_A           = $(APP_DIR)/app_A.c
SRC_APP_B_OVERLAY   = $(APP_DIR)/app_B_overlay.c
SRC_CLIENT_LIB      = $(LIB_DIR)/client_lib.c

# 目标文件
PROD_COMPOSITOR     = $(BUILD_DIR)/compositor.elf
PROD_APP_A          = $(BUILD_DIR)/app_A.elf
PROD_APP_B_OVERLAY  = $(BUILD_DIR)/app_B_overlay.elf
OBJ_CLIENT_LIB      = $(BUILD_DIR)/client_lib.o

TARGETS = $(PROD_COMPOSITOR) $(PROD_APP_A) $(PROD_APP_B_OVERLAY)


###########################################################
# 3. 编译规则 (Build Rules)
###########################################################

.PHONY: all clean

all: $(TARGETS)
	@echo "All targets built successfully with minimal dependencies."

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# 规则：编译客户端库，它没有任何外部依赖
$(OBJ_CLIENT_LIB): $(SRC_CLIENT_LIB) | $(BUILD_DIR)
	@echo "  CC (lib)  $(LIB_DIR)/client_lib.c"
	$(CC) $(CFLAGS) -c $(SRC_CLIENT_LIB) -o $@

# 规则：编译合成器，链接核心图形库
$(PROD_COMPOSITOR): $(SRC_COMPOSITOR) $(COMPOSITOR_DEPS_SRC) | $(BUILD_DIR)
	@echo "  LINK (compositor) $@"
	$(CC) $(CFLAGS) $(SRC_COMPOSITOR) $(COMPOSITOR_DEPS_SRC) -o $@ $(GFX_LDFLAGS)

# 规则：编译客户端A，只链接它自己的库和最基础的系统库
$(PROD_APP_A): $(SRC_APP_A) $(OBJ_CLIENT_LIB) | $(BUILD_DIR)
	@echo "  LINK (app) $@"
	$(CC) $(CFLAGS) $(SRC_APP_A) $(OBJ_CLIENT_LIB) -o $@ $(CLIENT_LDFLAGS)

# 规则：编译客户端B，同上
$(PROD_APP_B_OVERLAY): $(SRC_APP_B_OVERLAY) $(OBJ_CLIENT_LIB) | $(BUILD_DIR)
	@echo "  LINK (app) $@"
	$(CC) $(CFLAGS) $(SRC_APP_B_OVERLAY) $(OBJ_CLIENT_LIB) -o $@ $(CLIENT_LDFLAGS)

clean:
	@echo "Cleaning up..."
	rm -rf build