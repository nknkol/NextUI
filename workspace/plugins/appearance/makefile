###########################################################
# 从构建环境检查 PLATFORM 和 CROSS_COMPILE
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
# 包含通用的环境变量 (用于 ARCH, CFLAGS, LDFLAGS 等)
include ../../lib/libcommon/makefile.env

###########################################################

# 插件的目标名称
TARGET = appearance

# 源文件 (C++)
SOURCE = $(TARGET).cpp menu.cpp

# C++ 编译器
CXX = $(CROSS_COMPILE)g++

# 头文件包含目录: 当前目录, libcommon, 以及 minui (用于 C++ 菜单系统)
INCDIR = -I. -I../../lib/libcommon/

# C++ 编译器标志
# 继承来自 makefile.env 的 CFLAGS，并添加 C++17 标准支持
# -fPIC 是编译共享库所必需的
CXXFLAGS += $(CFLAGS) -fPIC -std=c++17
CXXFLAGS += $(INCDIR)

# 链接器标志
# libcommon 和 msettings 是必须的。-lstdc++ 用于链接 C++ 标准库
# 其他库如 SDL 假定已在 makefile.env 中定义
LDFLAGS += -lcommon -lmsettings -lstdc++

# 最终生成的文件路径和名称
PRODUCT= build/$(PLATFORM)/$(TARGET).so

# 默认构建规则
all:
	@echo "Building Appearance Plugin for $(PLATFORM)..."
	mkdir -p build/$(PLATFORM)
	$(CXX) $(SOURCE) -o $(PRODUCT) $(CXXFLAGS) $(LDFLAGS) -shared
	@echo "Successfully created $(PRODUCT)"

# 清理规则
clean:
	rm -rf build

.PHONY: all clean