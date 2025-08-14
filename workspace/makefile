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
	cd ./lib && make
	cd ./apps && make
	cd ./system && make
	cd ./cores && make
	cd ./tools && make
	cd ./other && make

clean:
	cd ./lib && make clean
	cd ./apps && make clean
	cd ./system && make clean
	cd ./cores && make clean
	cd ./tools && make clean
	cd ./other && make clean