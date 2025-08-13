###########################################################
ifeq (,$(PLATFORM))
PLATFORM=$(UNION_PLATFORM)
endif
ifeq (,$(PLATFORM))
$(error please specify PLATFORM, eg. PLATFORM=trimui make)
endif
###########################################################

.PHONY: all

all:
	cd ./libmsettings && make
	cd ./libcommon && make
	cd ./libwifimg && make
	cd ./libwifid && make
	cd ./libbatmondb && make
	cd ./libgametimedb && make

clean:
	cd ./libmsettings && make clean
	cd ./libcommon && make clean
	cd ./libwifimg && make clean
	cd ./libwifid && make clean
	cd ./libbatmondb && make clean
	cd ./libgametimedb && make clean