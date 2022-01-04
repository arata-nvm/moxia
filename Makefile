SHELL=/bin/bash

.PHONY: build
build:
	cd ~/edk2 \
	&& source edksetup.sh \
	&& build

.PHONY: run
run:
	~/osbook/devenv/run_qemu.sh ~/edk2/Build/MoxiaLoaderX64/DEBUG_CLANG38/X64/Loader.efi
