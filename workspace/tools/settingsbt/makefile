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

TARGET = settingsbt
COMPOSITOR_DIR = ../../system/compositor
CLIENT_LIB_DIR = $(COMPOSITOR_DIR)/client_lib
INCDIR = -I. -I../../lib/libcommon/ -I$(COMPOSITOR_DIR) -I$(CLIENT_LIB_DIR)
SOURCE = -c ../../lib/libcommon/lang.c \
../../lib/libcommon/utils.c \
../../lib/libcommon/api.c \
../../lib/libcommon/config.c \
../../lib/libcommon/scaler.c \
../../lib/libcommon/platform.c
CXXSOURCE = settings.cpp \
menu.cpp \
wifimenu.cpp \
btmenu.cpp \
keyboardprompt.cpp \
build/$(PLATFORM)/utils.o \
build/$(PLATFORM)/api.o \
build/$(PLATFORM)/config.o \
build/$(PLATFORM)/scaler.o \
build/$(PLATFORM)/platform.o \
build/$(PLATFORM)/lang.o \
$(CLIENT_LIB_DIR)/client_lib.c

CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
CFLAGS += $(ARCH) -fomit-frame-pointer
CFLAGS += $(INCDIR) -DPLATFORM=\"$(PLATFORM)\"
CXXFLAGS += $(CFLAGS) -std=c++17
CFLAGS += -fpermissive
LDFLAGS +=  -lmsettings
ifeq ($(PLATFORM), tg5040)
CFLAGS += -DHAS_WIFIMG -DHAS_BTMG
LDFLAGS +=  -lwifimg -lwifid -lEGL -lGLESv2
LDFLAGS +=  -lbtmg -lglib-2.0 -lgio-2.0 -lshared-mainloop -lbluetooth-internal -lasound -ljson-c -lrt -luapi
endif

PRODUCT= build/$(PLATFORM)/$(TARGET).elf

all: #$(PREFIX_LOCAL)/include/msettings.h
	mkdir -p build/$(PLATFORM)
	$(CC) $(SOURCE) $(CFLAGS) $(LDFLAGS)
	mv lang.o utils.o api.o config.o scaler.o platform.o build/$(PLATFORM)
	$(CXX) $(CXXSOURCE) -o $(PRODUCT) $(CXXFLAGS) $(LDFLAGS) -lstdc++
clean:
	rm -f $(PRODUCT)

# $(PREFIX_LOCAL)/include/msettings.h:
# 	cd ../../$(PLATFORM)/libmsettings && make