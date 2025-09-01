###########################################################
ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif
ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=rgb30 make)
endif
###########################################################

REQUIRES_EVTEST=evtest
REQUIRES_JSTEST=jstest
REQUIRES_UNZIP60=unzip60
REQUIRES_FILEMGR=NextCommander
REQUIRES_LIBTSM=libtsm

all: $(REQUIRES_EVTEST) $(REQUIRES_JSTEST) $(REQUIRES_UNZIP60) $(REQUIRES_FILEMGR) $(REQUIRES_LIBTSM)
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

clean:
	cd $(REQUIRES_FILEMGR) && make clean
	cd $(REQUIRES_UNZIP60) && make -f unix/Makefile.trimuismart clean

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
# include /readmes/makefile
