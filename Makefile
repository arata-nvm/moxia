SHELL=/bin/bash

.PHONY: run
run: build
	~/osbook/devenv/run_mikanos.sh

.PHONY: debug
debug: QEMU_OPTS += -s -S -no-reboot
debug: run

.PHONY: build
build: Loader.efi kernel.elf

.FORCE:

Loader.efi:
	cd ~/edk2 \
	&& source edksetup.sh \
	&& build
	cp ~/edk2/Build/MoxiaLoaderX64/DEBUG_CLANG38/X64/Loader.efi ./

kernel.elf: .FORCE
	source ~/osbook/devenv/buildenv.sh \
	&& make -C kernel
	cp kernel/kernel.elf ./
