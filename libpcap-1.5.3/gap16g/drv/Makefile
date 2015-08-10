XPCIE_HOME := $(shell pwd)
XPCIE_KVER := $(shell uname -r)
XPCIE_ARCH := $(shell uname -m)

EXTRA_CFLAGS	+=-DKERNEL_26X -D__KERNEL__ -D__linux__

obj-m += nac.o nacmem.o
nacmem-objs = nac_mem.o nacpci.o
nac-objs = nac_main.o nacts.o nacpci.o

ifneq ($(XPCIE_ARCH), x86_64)
obj-m += divdi3.o
divdi3-objs = divdi3_exp.o divdi3_impl.o
else
EXTRA_CFLAGS	+=-DPLATFORM64
endif


all:
	make -C /lib/modules/$(XPCIE_KVER)/build M=$(XPCIE_HOME) modules
	chmod +x ./nacload

clean:
	make -C /lib/modules/$(XPCIE_KVER)/build M=$(XPCIE_HOME) clean
