#include "asmfunc.hpp"
#include "console.hpp"
#include "fonts.hpp"
#include "graphics.hpp"
#include "memory_map.hpp"
#include "paging.hpp"
#include "segment.hpp"
#include <cstdint>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

void *operator new(size_t size, void *buf) {
  return buf;
}
void operator delete(void *obj) noexcept {}

extern "C" caddr_t sbrk(int incr) {
  return NULL;
}

char pixel_writer_buf[sizeof(RGBResv8BitPixelWriter)];
PixelWriter *pixel_writer;

char console_buf[sizeof(Console)];
Console *console;

int printk(const char *format...) {
  va_list ap;
  int result;
  char s[1024];

  va_start(ap, format);
  result = vsprintf(s, format, ap);
  va_end(ap);

  console->PutString(s);
  return result;
}

alignas(16) uint8_t kernel_main_stack[1024 * 1024];

extern "C" void KernelMainNewStack(const FrameBufferConfig &frame_buffer_config, const MemoryMap &memory_map) {
  switch (frame_buffer_config.pixel_format) {
  case kPixelRGBResv8BitPerColor:
    pixel_writer = new (pixel_writer_buf) RGBResv8BitPixelWriter{frame_buffer_config};
    break;
  case kPixelBGRResv8BitPerColor:
    pixel_writer = new (pixel_writer_buf) BGRResv8BitPixelWriter{frame_buffer_config};
    break;
  }

  console = new (console_buf) Console(*pixel_writer, {255, 255, 255}, {0, 0, 0});

  printk("Hello, world!\n");

  SetupSegments();

  const uint16_t kernel_cs = 1 << 3;
  const uint16_t kernel_ss = 2 << 3;
  SetDSAll(0);
  SetCSSS(kernel_cs, kernel_ss);

  SetupIdentityPageTable();

  while (1)
    __asm__("hlt");
}
