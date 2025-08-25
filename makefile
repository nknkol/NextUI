# =============================================================================
# 配置部分 (Configuration)
# =============================================================================

# 如果未在外部指定，则设置默认平台
PLATFORM ?= tg5040
# 编译时使用的核心数
COMPILE_CORES ?= 8

# 构建元数据
BUILD_HASH   := $(shell git rev-parse --short HEAD)
BUILD_BRANCH := $(shell git rev-parse --abbrev-ref HEAD)
RELEASE_TIME := $(shell TZ=GMT date +%Y%m%d)

# 根据分支和现有文件确定发布名称
ifeq ($(BUILD_BRANCH),main)
  RELEASE_BETA :=
else
  RELEASE_BETA := -$(BUILD_BRANCH)
endif
RELEASE_BASE := NextUI-$(RELEASE_TIME)$(RELEASE_BETA)
# 计算当前基础名称的发布版本号（例如 -0, -1, ...）
RELEASE_DOT  := $(shell find ./releases/. -maxdepth 1 -type f -name "$(RELEASE_BASE)-*-base.zip" | wc -l | tr -d ' ')
# 最终的发布名称，可以被外部覆盖
RELEASE_NAME ?= $(RELEASE_BASE)-$(RELEASE_DOT)

# 工具链文件
TOOLCHAIN_FILE := makefile.toolchain

# 禁止输出 "Entering/Leaving directory" 等信息，使输出更整洁
export MAKEFLAGS += --no-print-directory

# =============================================================================
# 路径定义 (Path Definitions)
# =============================================================================
# 源文件路径
WORKSPACE_DIR := ./workspace
SRC_LIB_DIR   := $(WORKSPACE_DIR)/lib
SRC_APP_DIR   := $(WORKSPACE_DIR)/apps
SRC_SYS_DIR   := $(WORKSPACE_DIR)/system
SRC_CORE_DIR  := $(WORKSPACE_DIR)/cores/output
SRC_TOOL_DIR  := $(WORKSPACE_DIR)/tools
SRC_OTHER_DIR := $(WORKSPACE_DIR)/other
SRC_PLUGINS_DIR := $(WORKSPACE_DIR)/plugins

# 构建（目标）路径
BUILD_DIR      := ./build
RELEASE_DIR    := ./releases
SYSTEM_DIR     := $(BUILD_DIR)/SYSTEM
SYSTEM_BIN     := $(SYSTEM_DIR)/bin
SYSTEM_LIB     := $(SYSTEM_DIR)/lib
PLUGINS_DIR    := $(SYSTEM_DIR)/plugins
SYSTEM_CORES   := $(SYSTEM_DIR)/cores
BOOT_DIR       := $(BUILD_DIR)/BOOT
EXTRAS_DIR     := $(BUILD_DIR)/EXTRAS
EXTRAS_EMUS    := $(EXTRAS_DIR)/Emus
EXTRAS_TOOLS   := $(EXTRAS_DIR)/Tools

# =============================================================================
# 目标定义 (Targets)
# =============================================================================

# .PHONY 用于声明伪目标，避免与同名文件冲突
.PHONY: all build lib apps system cores tools cpfile clean setup done special tidy package name shell push help

# 默认目标：执行 setup, build, special, package, done 全流程
all: setup build special package done
# 构建所有内容，包括模拟器核心
all-cores: setup build-cores special package done

# 帮助目标：显示可用的命令和说明
help:
	@echo "用法: make [目标] [变量=值]"
	@echo ""
	@echo "主要目标:"
	@echo "  all           - 构建主程序。模拟器核心(cores)必须事先编译好。"
	@echo "  all-cores     - 构建所有内容，包括模拟器核心。"
	@echo "  package       - 为发布版本创建 zip 压缩包。"
	@echo "  push          - 通过 adb 推送程序到设备。用法: make push PROGRAM=my.elf"
	@echo "  shell         - 进入指定平台的工具链 shell 环境。"
	@echo "  clean         - 清理所有生成的文件和构建目录。"
	@echo ""
	@echo "可用变量:"
	@echo "  PLATFORM      - 目标平台 (例如: tg5040)。默认值: tg5040"
	@echo "  COMPILE_CORES - 编译时使用的核心数。默认值: 8"

# 主要构建目标，调用工具链 makefile
build:
	make build -f $(TOOLCHAIN_FILE) PLATFORM=$(PLATFORM) COMPILE_CORES=$(COMPILE_CORES)

# 构建模拟器核心
build-cores:
	make build-cores -f $(TOOLCHAIN_FILE) PLATFORM=$(PLATFORM) COMPILE_CORES=true

# 将所有必要的文件复制到构建目录中
cpfile: lib apps system cores tools plugins

# --- 文件复制子目标 ---

lib:
	cp $(SRC_LIB_DIR)/libwifid/libwifid.so $(SYSTEM_LIB)/
	cp $(SRC_APP_DIR)/minarch/build/$(PLATFORM)/libsamplerate.* $(SYSTEM_LIB)/
	cp $(SRC_APP_DIR)/minarch/build/$(PLATFORM)/libzip.* $(SYSTEM_LIB)/
	cp $(SRC_APP_DIR)/minarch/build/$(PLATFORM)/libbz2.* $(SYSTEM_LIB)/
	cp $(SRC_APP_DIR)/minarch/build/$(PLATFORM)/liblzma.* $(SYSTEM_LIB)/
	cp $(SRC_APP_DIR)/minarch/build/$(PLATFORM)/libzstd.* $(SYSTEM_LIB)/
	cp $(SRC_LIB_DIR)/libcommon/libcommon.so $(SYSTEM_LIB)/
	cp $(SRC_LIB_DIR)/libmsettings/libmsettings.so $(SYSTEM_LIB)/
	cp $(SRC_LIB_DIR)/libbatmondb/build/$(PLATFORM)/libbatmondb.so $(SYSTEM_LIB)/
	cp $(SRC_LIB_DIR)/libgametimedb/build/$(PLATFORM)/libgametimedb.so $(SYSTEM_LIB)/

	# cp $(SRC_SYS_DIR)/compositor/build/$(PLATFORM)/libfb_compositor.so $(EXTRAS_TOOLS)/Compositor.pak/

apps:
	cp $(SRC_APP_DIR)/nextui/build/$(PLATFORM)/nextui.elf $(SYSTEM_BIN)/
	cp $(SRC_APP_DIR)/minarch/build/$(PLATFORM)/minarch.elf $(SYSTEM_BIN)/

system:
	# 系统工具和安装脚本
	cp $(SRC_SYS_DIR)/show/show.elf $(SYSTEM_BIN)/
	cp $(SRC_OTHER_DIR)/install/boot.sh $(BOOT_DIR)/common/$(PLATFORM).sh
	cp $(SRC_OTHER_DIR)/install/update.sh $(SYSTEM_BIN)/install.sh
	mkdir -p $(BOOT_DIR)/common/$(PLATFORM)/
	cp $(SRC_OTHER_DIR)/install/*.png $(BOOT_DIR)/common/$(PLATFORM)/
	cp -r $(SRC_OTHER_DIR)/install/brick $(BOOT_DIR)/common/$(PLATFORM)/
	cp $(SRC_SYS_DIR)/show/show.elf $(BOOT_DIR)/common/$(PLATFORM)/
	cp $(SRC_OTHER_DIR)/unzip60/unzip $(BOOT_DIR)/common/$(PLATFORM)/
	cp $(SRC_SYS_DIR)/wifimanager/daemon/wifi_daemon $(SYSTEM_BIN)/
	cp $(SRC_SYS_DIR)/nextval/build/$(PLATFORM)/nextval.elf $(SYSTEM_BIN)/
	cp $(SRC_SYS_DIR)/keymon/keymon.elf $(SYSTEM_BIN)/
	cp $(SRC_SYS_DIR)/gametimectl/build/$(PLATFORM)/gametimectl.elf $(SYSTEM_BIN)/
	cp $(SRC_SYS_DIR)/batmon/build/$(PLATFORM)/batmon.elf $(SYSTEM_BIN)/


# 使用列表和 foreach 循环简化核心文件的复制过程
STOCK_CORES  := fceumm gambatte gpsp picodrive snes9x pcsx_rearmed
CORE_SUFFIX  := _libretro.so
cores:
	@# 在尝试复制前，检查一个代表性的核心文件是否存在
	if [ -f "$(SRC_CORE_DIR)/fceumm_libretro.so" ]; then \
		echo "检测到已编译的核心文件，开始复制..."; \
		$(foreach core,$(STOCK_CORES), cp $(SRC_CORE_DIR)/$(core)$(CORE_SUFFIX) $(SYSTEM_CORES)/;) \
		cp $(SRC_CORE_DIR)/a5200_libretro.so $(EXTRAS_EMUS)/A5200.pak; \
		cp $(SRC_CORE_DIR)/prosystem_libretro.so $(EXTRAS_EMUS)/A7800.pak; \
		cp $(SRC_CORE_DIR)/stella2014_libretro.so $(EXTRAS_EMUS)/A2600.pak; \
		cp $(SRC_CORE_DIR)/handy_libretro.so $(EXTRAS_EMUS)/LYNX.pak; \
		cp $(SRC_CORE_DIR)/mgba_libretro.so $(EXTRAS_EMUS)/MGBA.pak; \
		cp $(SRC_CORE_DIR)/mgba_libretro.so $(EXTRAS_EMUS)/SGB.pak; \
		cp $(SRC_CORE_DIR)/mednafen_pce_fast_libretro.so $(EXTRAS_EMUS)/PCE.pak; \
		cp $(SRC_CORE_DIR)/pokemini_libretro.so $(EXTRAS_EMUS)/PKM.pak; \
		cp $(SRC_CORE_DIR)/race_libretro.so $(EXTRAS_EMUS)/NGP.pak; \
		cp $(SRC_CORE_DIR)/race_libretro.so $(EXTRAS_EMUS)/NGPC.pak; \
		cp $(SRC_CORE_DIR)/fbneo_libretro.so $(EXTRAS_EMUS)/FBN.pak; \
		cp $(SRC_CORE_DIR)/cap32_libretro.so $(EXTRAS_EMUS)/CPC.pak; \
		cp $(SRC_CORE_DIR)/puae2021_libretro.so $(EXTRAS_EMUS)/PUAE.pak; \
		cp $(SRC_CORE_DIR)/prboom_libretro.so $(EXTRAS_EMUS)/PRBOOM.pak; \
		cp $(SRC_CORE_DIR)/vice_x64_libretro.so $(EXTRAS_EMUS)/C64.pak; \
		cp $(SRC_CORE_DIR)/vice_x128_libretro.so $(EXTRAS_EMUS)/C128.pak; \
		cp $(SRC_CORE_DIR)/vice_xplus4_libretro.so $(EXTRAS_EMUS)/PLUS4.pak; \
		cp $(SRC_CORE_DIR)/vice_xpet_libretro.so $(EXTRAS_EMUS)/PET.pak; \
		cp $(SRC_CORE_DIR)/vice_xvic_libretro.so $(EXTRAS_EMUS)/VIC.pak; \
	else \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"; \
		echo "!! 错误：模拟器核心未编译。"; \
		echo "!! 请至少运行一次 'make all-cores' 来编译它们。"; \
		echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"; \
		exit 1; \
	fi

tools:
	cp $(SRC_OTHER_DIR)/NextCommander/output/NextCommander $(EXTRAS_TOOLS)/Files.pak/
	cp -r $(SRC_OTHER_DIR)/NextCommander/res $(EXTRAS_TOOLS)/Files.pak/
	cp $(SRC_TOOL_DIR)/clock/build/$(PLATFORM)/clock.elf $(EXTRAS_TOOLS)/Clock.pak/
	cp $(SRC_TOOL_DIR)/minput/build/$(PLATFORM)/minput.elf $(EXTRAS_TOOLS)/Input.pak/
	cp $(SRC_TOOL_DIR)/battery/build/$(PLATFORM)/battery.elf $(EXTRAS_TOOLS)/Battery.pak/
	cp $(SRC_TOOL_DIR)/gametime/build/$(PLATFORM)/gametime.elf $(EXTRAS_TOOLS)/Game\ Tracker.pak/
	cp $(SRC_TOOL_DIR)/settings/build/$(PLATFORM)/settings.elf $(EXTRAS_TOOLS)/Settings.pak/
	cp $(SRC_TOOL_DIR)/ledcontrol/build/$(PLATFORM)/ledcontrol.elf $(EXTRAS_TOOLS)/LedControl.pak/
	cp $(SRC_TOOL_DIR)/bootlogo/build/$(PLATFORM)/bootlogo.elf $(EXTRAS_TOOLS)/Bootlogo.pak/

	# cp $(SRC_SYS_DIR)/compositor/build/$(PLATFORM)/compositor.elf $(EXTRAS_TOOLS)/Compositor.pak/
	# cp $(SRC_TOOL_DIR)/demo1_overlay/build/$(PLATFORM)/overlay.elf $(EXTRAS_TOOLS)/Compositor.pak/
	# cp $(SRC_TOOL_DIR)/demo2_background/build/$(PLATFORM)/background.elf $(EXTRAS_TOOLS)/Compositor.pak/

plugins:
	cp $(SRC_PLUGINS_DIR)/clock/build/$(PLATFORM)/clock.so $(PLUGINS_DIR)/

# --- 主要工作流程目标 ---

setup: name
	# 准备一个全新的构建目录
	@echo "正在为发布版本 $(RELEASE_NAME) 进行初始化..."
	rm -rf $(BUILD_DIR)
	mkdir -p $(RELEASE_DIR)
	cp -R ./skeleton $(BUILD_DIR)
	# 清理占位符和元数据文件
	find $(BUILD_DIR) -type f \( -name '.keep' -o -name '*.meta' \) -delete
	echo $(BUILD_HASH) > $(SRC_OTHER_DIR)/readmes/hash.txt
	# 复制 readme 文件以供后续处理
	cp ./skeleton/BASE/README.txt $(SRC_OTHER_DIR)/readmes/BASE-in.txt
	cp ./skeleton/EXTRAS/README.txt $(SRC_OTHER_DIR)/readmes/EXTRAS-in.txt

special: cpfile
	# Miyoomini 系列设备的特殊文件结构设置
	mv $(BOOT_DIR)/common $(BOOT_DIR)/.tmp_update
	mv $(BOOT_DIR)/trimui $(BUILD_DIR)/BASE/
	cp -R $(BOOT_DIR)/.tmp_update $(BUILD_DIR)/BASE/trimui/app/

package: tidy
	# 完成并压缩发布文件
	# 将格式化后的 readme 文件移动到构建目录
	cp $(SRC_OTHER_DIR)/readmes/BASE-out.txt $(BUILD_DIR)/BASE/README.txt
	cp $(SRC_OTHER_DIR)/readmes/EXTRAS-out.txt $(BUILD_DIR)/EXTRAS/README.txt
	# 创建版本文件
	cd $(SYSTEM_DIR) && echo "$(RELEASE_NAME)\n$(BUILD_HASH)" > version.txt
	find $(BUILD_DIR) -type f -name '.DS_Store' -delete
	# 创建 MinUI.zip 更新包
	mkdir -p $(BUILD_DIR)/PAYLOAD
	mv $(SYSTEM_DIR) $(BUILD_DIR)/PAYLOAD/.system
	cp -R $(BOOT_DIR)/.tmp_update $(BUILD_DIR)/PAYLOAD/
	cp -R $(EXTRAS_TOOLS) $(BUILD_DIR)/PAYLOAD/
	cd $(BUILD_DIR)/PAYLOAD && zip -r MinUI.zip .system .tmp_update Tools
	mv $(BUILD_DIR)/PAYLOAD/MinUI.zip $(BUILD_DIR)/BASE
	# 创建最终的发布压缩包
	cd $(BUILD_DIR)/BASE && zip -r ../../$(RELEASE_DIR)/$(RELEASE_NAME)-base.zip .
	cd $(BUILD_DIR)/EXTRAS && zip -r ../../$(RELEASE_DIR)/$(RELEASE_NAME)-extras.zip .
	cd $(RELEASE_DIR) && zipmerge $(RELEASE_NAME)-all.zip $(RELEASE_NAME)-base.zip $(RELEASE_NAME)-extras.zip

tidy:
	# 清理旧的发布压缩包
	rm -f $(RELEASE_DIR)/$(RELEASE_NAME)-*.zip

# --- 实用工具目标 ---

clean:
	# 清理构建目录和工作空间
	rm -rf $(BUILD_DIR)
	cd $(WORKSPACE_DIR) && make clean PLATFORM=$(PLATFORM) COMPILE_CORES=$(COMPILE_CORES)

done:
	@echo "构建流程结束。"

name:
	# 打印将要生成的发布名称
	@echo $(RELEASE_NAME)

shell:
	# 进入工具链 shell
	make -f $(TOOLCHAIN_FILE) PLATFORM=$(PLATFORM)

# =============================================================================
# ADB 推送 (push) 功能定义
# =============================================================================
nextui_SRC      := workspace/apps/nextui/build/tg5040/nextui.elf
nextui_DEST     := /mnt/SDCARD/.system/bin
nextui_COMMANDS := @echo "--> 正在推送 nextui..." && \
                   adb push $(nextui_SRC) $(nextui_DEST) && \
				   adb reboot

minarch_SRC      := workspace/other_apps/minarch/build/tg5040/minarch.elf
minarch_DEST     := /mnt/SDCARD/.system/bin
minarch_COMMANDS := @echo "--> 正在推送 minarch..." && \
                    adb push $(minarch_SRC) $(minarch_DEST)

libcommon_SRC      := workspace/lib/libcommon/libcommon.so
clockplugin_SRC    := workspace/plugins/clock/build/tg5040/clock.so
pluginlib_DEST     := /mnt/SDCARD/.system/plugins
lib_DEST           := /mnt/SDCARD/.system/lib
lib_COMMANDS       := @echo "--> 正在推送库文件及插件..." && \
                      adb push $(libcommon_SRC) $(lib_DEST) && \
				      adb push $(clockplugin_SRC) $(pluginlib_DEST)


compositor_SRC      := workspace/system/compositor/build/tg5040/compositor.elf
libfb_compositor_SRC := workspace/system/compositor/build/tg5040/libfb_compositor.so
demo1_overlay_SRC    := workspace/tools/demo1_overlay/build/tg5040/overlay.elf
demo2_background_SRC := workspace/tools/demo2_background/build/tg5040/background.elf
compositorlaunch_SRC := skeleton/EXTRAS/Tools/Compositor.pak/launch.sh
compositor_DEST     := /mnt/SDCARD/Tools/Compositor.pak/
compositor_COMMANDS       := @echo "--> 正在推送compositor..." && \
                      adb push $(compositor_SRC) $(compositor_DEST) && \
				      adb push $(libfb_compositor_SRC) $(compositor_DEST) && \
					  adb push $(demo1_overlay_SRC) $(compositor_DEST) && \
					  adb push $(demo2_background_SRC) $(compositor_DEST) && \
					  adb push $(compositorlaunch_SRC) $(compositor_DEST)

nextui_SRC      := workspace/apps/nextui/build/tg5040/nextui.elf
default_COMMANDS := echo "--> 正在构建并打包以供更新 (默认操作)..." && \
                    make all && \
                    echo "--> 正在推送 MinUI.zip 到设备的 SD 卡..." && \
                    adb push $(BUILD_DIR)/BASE/MinUI.zip /mnt/SDCARD/ && \
					adb reboot && \
                    echo "--> 更新包推送完成。"

# 根据 PROGRAM 变量选择最终执行的命令
PROGRAM ?= default
FINAL_COMMANDS := $($(PROGRAM)_COMMANDS)

push:
	@if [ "$(origin $(PROGRAM)_COMMANDS)" = "undefined" ]; then \
		echo "错误: 未找到 PROGRAM='$(PROGRAM)' 的推送配置。"; \
		echo "可用配置: default, nextui, minarch, lib, compositor"; \
		exit 1; \
	fi
	# 在这里加上 @，让 make 来处理命令回显
	@$($(PROGRAM)_COMMANDS)
	@echo "--> 操作完成。"
# =============================================================================
