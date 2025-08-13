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
	cd ./minarch && make
	cd ./nextui && make

clean:
	cd ./minarch && make clean
	cd ./nextui && make clean