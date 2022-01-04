.PHONY: build
build:
	clang++ ${CPPFLAGS} -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone -fno-exceptions -fno-rtti -std=c++17 -c Main.cpp
	ld.lld ${LDFLAGS} --entry KernelMain -z norelro --image-base 0x100000 --static -o kernel.elf Main.o
