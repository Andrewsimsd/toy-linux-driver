.PHONY: all app driver clean

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all: app driver

app:
	cargo build

driver:
	$(MAKE) -C $(KDIR) M=$(PWD)/driver modules

clean:
	cargo clean
	$(MAKE) -C $(KDIR) M=$(PWD)/driver clean
