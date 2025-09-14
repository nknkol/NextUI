###########################################################
ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif
ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=rgb30 make)
endif
###########################################################

# --- 用于下载 Fish 官方版本的变量 ---
FISH_VERSION=4.0.6
FISH_ARCH=linux-aarch64
FISH_TARBALL=fish-$(FISH_VERSION)-$(FISH_ARCH).tar.xz
FISH_URL=https://github.com/fish-shell/fish-shell/releases/download/$(FISH_VERSION)/$(FISH_TARBALL)
FISH_OFFICIAL_BUILD_DIR=fish-shell/official_build

# --- 原有变量 ---
REQUIRES_EVTEST=evtest
REQUIRES_JSTEST=jstest
REQUIRES_UNZIP60=unzip60
REQUIRES_FILEMGR=NextCommander
REQUIRES_LIBTSM=libtsm
REQUIRES_FISH=fish-shell
STRACE=strace

all: $(REQUIRES_EVTEST) $(REQUIRES_JSTEST) $(REQUIRES_UNZIP60) $(REQUIRES_FILEMGR) $(REQUIRES_LIBTSM) $(REQUIRES_FISH) $(STRACE)
	cd readmes && make
	cd $(REQUIRES_FILEMGR) && make -j
	cd $(REQUIRES_EVTEST) && $(CROSS_COMPILE)gcc -o evtest evtest.c
	cd $(REQUIRES_JSTEST) && $(CROSS_COMPILE)gcc -o jstest jstest.c
	cd $(REQUIRES_UNZIP60) && make -f unix/Makefile.trimuismart unzip
	@echo "Building libtsm (native compilation)..."
	cd $(REQUIRES_LIBTSM) && \
		if [ ! -f build/build.ninja ]; then \
			meson setup build \
				--default-library=static \
				-Dtests=false \
				-Dextra_debug=false \
				-Dgtktsm=false; \
		fi && \
		cd build && meson compile && \
		cp ../src/tsm/libtsm.h "$(PREFIX_LOCAL)/include" && \
		cp src/tsm/libtsm.so.4.1.0 "$(PREFIX_LOCAL)/lib/libtsm.so"

	@echo "Building fish-shell language files..."
	cd $(REQUIRES_FISH) && \
		if [ ! -d build ]; then mkdir -p build; fi && \
		cd build && \
		cmake .. -DWITH_GETTEXT=ON && \
		make pofiles # <--- 最终修正！

	# --- strace 部分保持不变 ---
	cd $(STRACE) && \
		./bootstrap && \
		./configure \
		--enable-static \
		--disable-shared \
		--enable-mpers=no \
		--enable-bundled=yes \
		LDFLAGS="-static" \
		LIBS="-lrt -lm -lpthread" && \
		make

clean:
	cd $(REQUIRES_FILEMGR) && make clean
	cd $(REQUIRES_UNZIP60) && make -f unix/Makefile.trimuismart clean
	if [ -d $(REQUIRES_FISH)/build ]; then cd $(REQUIRES_FISH)/build && make clean; fi
	rm -rf $(FISH_OFFICIAL_BUILD_DIR)

###########################################################
# 依赖目标部分
###########################################################

$(REQUIRES_FILEMGR):
	git clone --depth 1 https://github.com/LoveRetro/NextCommander.git $(REQUIRES_FILEMGR)
	
$(REQUIRES_EVTEST):
	git clone --depth 1 https://github.com/freedesktop-unofficial-mirror/evtest.git $(REQUIRES_EVTEST)

$(REQUIRES_JSTEST):
	git clone --depth 1 https://github.com/datrh/joyutils.git $(REQUIRES_JSTEST)

$(REQUIRES_UNZIP60):
	git clone --depth 1 https://github.com/shauninman/unzip60.git $(REQUIRES_UNZIP60)

$(REQUIRES_LIBTSM):
	git clone --depth 1 https://github.com/Aetf/libtsm.git $(REQUIRES_LIBTSM)

$(REQUIRES_FISH):
	# 1. 克隆源码，用于编译 .gmo 文件
	git clone --depth 1 --branch $(FISH_VERSION) https://github.com/fish-shell/fish-shell.git $(REQUIRES_FISH)
	# 2. 创建存放官方二进制文件的专用目录
	mkdir -p $(FISH_OFFICIAL_BUILD_DIR)
	# 3. 下载官方预编译版本到该目录
	if [ ! -f "$(FISH_OFFICIAL_BUILD_DIR)/$(FISH_TARBALL)" ]; then \
		wget -O $(FISH_OFFICIAL_BUILD_DIR)/$(FISH_TARBALL) $(FISH_URL); \
	fi
	# 4. 解压缩官方版本到该目录
	if [ ! -f "$(FISH_OFFICIAL_BUILD_DIR)/fish" ]; then \
		tar -xf $(FISH_OFFICIAL_BUILD_DIR)/$(FISH_TARBALL) -C $(FISH_OFFICIAL_BUILD_DIR); \
	fi

$(STRACE):
	git clone --depth 1 https://github.com/strace/strace.git $(STRACE)