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

extern "C" void KernelMain(const FrameBufferConfig &frame_buffer_config) {
  switch (frame_buffer_config.pixel_format) {
  case kPixelRGBResv8BitPerColor:
    pixel_writer = new (pixel_writer_buf) RGBResv8BitPixelWriter{frame_buffer_config};
    break;
  case kPixelBGRResv8BitPerColor:
    pixel_writer = new (pixel_writer_buf) BGRResv8BitPixelWriter{frame_buffer_config};
    break;
  }

  for (int x = 0; x < frame_buffer_config.horizontal_resolution; ++x) {
    for (int y = 0; y < frame_buffer_config.vertical_resolution; ++y) {
      pixel_writer->Write(x, y, {255, 255, 255});
    }
  }
  for (int x = 0; x < 200; ++x) {
    for (int y = 0; y < 100; ++y) {
      pixel_writer->Write(x, y, {0, 255, 0});
    }
  }

  for (char c = '!'; c <= '~'; ++c) {
    WriteAscii(*pixel_writer, 8 * (c - '!'), 50, c, {0, 0, 0});
  }
  WriteString(*pixel_writer, 0, 66, "Hello, world!", {0, 0, 255});

  while (1)
    __asm__("hlt");
}
