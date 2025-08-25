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
	cd ./keymon && make
	cd ./batmon && make
	cd ./gametimectl && make
	cd ./nextval && make
	cd ./syncsettings && make
	cd ./wifimanager && make
	cd ./show && make
	# cd ./compositor && make 

clean:
	cd ./keymon && make clean
	cd ./batmon && make clean
	cd ./gametimectl && make clean
	cd ./nextval && make clean
	cd ./syncsettings && make clean
	cd ./wifimanager && make clean
	cd ./show && make clean