#keymon

ifeq (,$(CROSS_COMPILE))
$(error missing CROSS_COMPILE for this toolchain)
endif

TARGET = llayerdemo


CC = $(CROSS_COMPILE)gcc
CFLAGS	= -Os -luapi -lpthread -lrt -ldl -Wl,--gc-sections -s
CFLAGS  += -I. -I../../all/common -I../platform/ -I$(PREFIX_LOCAL)/include -L/usr/lib -I/usr/include/allwinner/ -I/usr/include/allwinner/include/ -L$(PREFIX_LOCAL)/lib -DPLATFORM=\"$(UNION_PLATFORM)\"
# LDFLAGS	 += -luapi -static
all:
	$(CC) $(TARGET).c -o $(TARGET).elf $(CFLAGS)
clean:
	rm -rf $(TARGET).elf