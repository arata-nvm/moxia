TARGET = kernel.elf
OBJS = main.o fonts.o graphics.o hankaku.o console.o asmfunc.o paging.o segment.o memory_manager.o newlib_support.o libcxx_support.o printk.o interrupt.o timer.o task.o pic.o keyboard.o terminal.o fat.o syscall.o file.o

CFLAGS += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone
CXXFLAGS += -O2 -Wall -g --target=x86_64-elf -ffreestanding -mno-red-zone -fno-exceptions -fno-rtti -std=c++17
LDFLAGS += --entry KernelMain -z norelro --image-base 0x100000 --static

.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -rf *.o

kernel.elf: $(OBJS) Makefile
	ld.lld $(LDFLAGS) -o kernel.elf $(OBJS) -lc -lc++

%.o: %.c Makefile
	clang $(CPPFLAGS) $(CFLAGS) -c $<

%.o: %.cpp Makefile
	clang++ $(CPPFLAGS) $(CXXFLAGS) -c $<

%.o: %.asm Makefile
	nasm -f elf64 -o $@ $<

hankaku.o: hankaku.bin
	objcopy -I binary -O elf64-x86-64 -B i386:x86-64 $< $@
