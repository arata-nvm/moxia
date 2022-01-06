#include "console.hpp"
#include "fonts.hpp"
#include "graphics.hpp"
#include <cstdint>
#include <stddef.h>

void *operator new(size_t size, void *buf) {
  return buf;
}
void operator delete(void *obj) noexcept {}

char pixel_writer_buf[sizeof(RGBResv8BitPixelWriter)];
PixelWriter *pixel_writer;

char console_buf[sizeof(Console)];
Console *console;

extern "C" void KernelMain(const FrameBufferConfig &frame_buffer_config) {
  switch (frame_buffer_config.pixel_format) {
  case kPixelRGBResv8BitPerColor:
    pixel_writer = new (pixel_writer_buf) RGBResv8BitPixelWriter{frame_buffer_config};
    break;
  case kPixelBGRResv8BitPerColor:
    pixel_writer = new (pixel_writer_buf) BGRResv8BitPixelWriter{frame_buffer_config};
    break;
  }

  console = new (console_buf) Console(*pixel_writer, {0, 0, 0}, {255, 255, 255});

  while (true) {
    console->PutString("Test\n");
  }

  while (1)
    __asm__("hlt");
}
