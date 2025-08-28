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
	# cd ./battery && make
	cd ./bootlogo && make
	# cd ./clock && make
	# cd ./gametime && make
	cd ./ledcontrol && make
	cd ./minput && make
	cd ./settings && make

	# cd ./demo1_overlay && make 
	# cd ./demo2_background && make

clean:
	cd ./battery && make clean
	cd ./bootlogo && make clean
	cd ./clock && make clean
	cd ./gametime && make clean
	cd ./ledcontrol && make clean
	cd ./minput && make clean
	cd ./settings && make clean