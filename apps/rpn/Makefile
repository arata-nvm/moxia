TARGET = rpn
OBJS = rpn.o syscall.o newlib_support.o

SHELL=/bin/bash

CFLAGS += -O2 -Wall -g --target=x86_64-elf -ffreestanding
CXXFLAGS += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mcmodel=large \
							-fno-exceptions -fno-rtti -std=c++17
LDFLAGS += --entry main -z norelro --image-base 0xffff800000000000 --static

.PHONY: all
all: $(TARGET)

rpn: $(OBJS) Makefile
	source ~/osbook/devenv/buildenv.sh \
	&& ld.lld $(LDFLAGS) -o rpn $(OBJS) -lc -lc++ -lc++abi

%.o: %.c Makefile
	source ~/osbook/devenv/buildenv.sh \
	&& clang $(CPPFLAGS) $(CFLAGS) -c $<

%.o: %.cpp Makefile
	source ~/osbook/devenv/buildenv.sh \
	&& clang++ $(CPPFLAGS) $(CXXFLAGS) -c $<

%.o: %.asm Makefile
	nasm -f elf64 -o $@ $<
