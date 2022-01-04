SHELL=/bin/bash

.PHONY: run
run: build
	~/osbook/devenv/run_qemu.sh Loader.efi kernel.elf

.PHONY: build
build: Loader.efi kernel.elf

Loader.efi:
	cd ~/edk2 \
	&& source edksetup.sh \
	&& build
	cp ~/edk2/Build/MoxiaLoaderX64/DEBUG_CLANG38/X64/Loader.efi ./

kernel.elf:
	source ~/osbook/devenv/buildenv.sh \
	&& make -C kernel
	cp kernel/kernel.elf ./
