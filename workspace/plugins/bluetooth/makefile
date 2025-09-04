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
TARGET = btnetwork

SOURCE = $(TARGET).c keyboard.c

CC = $(CROSS_COMPILE)gcc

INCDIR = -I. -I../../lib/libcommon/

CXXFLAGS += $(CFLAGS) -fPIC -std=c++17
CXXFLAGS += $(INCDIR)

LDFLAGS += -lcommon -lmsettings -shared -lbtmg

PRODUCT= build/$(PLATFORM)/$(TARGET).so

# 默认构建规则
all:
	@echo "Building Bluetooth Plugin for $(PLATFORM)..."
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) -o $(PRODUCT) $(CFLAGS) $(INCDIR) -fPIC -shared $(LDFLAGS)
	@echo "Successfully created $(PRODUCT)"

# 清理规则
clean:
	rm -rf build

.PHONY: all clean