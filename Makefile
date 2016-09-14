all: iwl_parse

KERNEL = $(strip $(shell uname -r))
KERNEL_SOURCE = /lib/modules/$(KERNEL)/build

ifneq ($(wildcard $(KERNEL_SOURCE)/include/uapi),)
        KERNEL_HEADERS = $(KERNEL_SOURCE)/include/uapi
else ifneq ($(wildcard $(KERNEL_SOURCE)/include),)
        KERNEL_HEADERS = $(KERNEL_SOURCE)/include
else
        $(error Kernel headers not found)
endif

CFLAGS =  -Wall -I/usr/local/include 
#-Werror
LDLIBS = -lm -L. -lpthread -ldl
gcc = /home/lisa/Downloads/OpenWrt-Toolchain-ramips-for-mipsel_24kec+dsp-gcc-4.8-linaro_uClibc-0.9.33.2/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_uClibc-0.9.33.2/bin/mipsel-openwrt-linux-gcc
# CC = gcc

iwl_parse: iwl_parse.o iwl_nl.o analyze.o filter.o

iwl_parse.c: iwl_connector.h

iwl_nl.c: iwl_connector.h

iwl_connector.h: connector_users.h

connector_users.h: $(KERNEL_HEADERS)/linux/connector.h
	echo "#undef CN_NETLINK_USERS" > connector_users.h
	grep "#define CN_NETLINK_USERS" $(KERNEL_HEADERS)/linux/connector.h >> connector_users.h

clean:
	rm -f *.o connector_users.h iwl_parse send_csi
