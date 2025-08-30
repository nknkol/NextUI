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
include ../../lib/libcommon/makefile.env
###########################################################

# 插件的目标名称
TARGET = network

SOURCE = $(TARGET).cpp menu.cpp wifimenu.cpp keyboardprompt.cpp

CXX = $(CROSS_COMPILE)g++

INCDIR = -I. -I../../lib/libcommon/

CXXFLAGS += $(CFLAGS) -fPIC -std=c++17
CXXFLAGS += $(INCDIR)

LDFLAGS += -lcommon -lmsettings -lstdc++ -shared -lwifimg -lwifid

PRODUCT= build/$(PLATFORM)/$(TARGET).so

# 默认构建规则
all:
	@echo "Building Network Plugin for $(PLATFORM)..."
	mkdir -p build/$(PLATFORM)
	$(CXX) $(SOURCE) -o $(PRODUCT) $(CXXFLAGS) $(LDFLAGS) -shared
	@echo "Successfully created $(PRODUCT)"

# 清理规则
clean:
	rm -rf build

.PHONY: all clean