#include "graphics.hpp"
#include <new>

void PixelWriter::Clear(const PixelColor &c) {
  for (int y = 0; y < config_.vertical_resolution; y++) {
    for (int x = 0; x < config_.horizontal_resolution; x++) {
      this->Write(x, y, c);
    }
  }
}

void RGBResv8BitPixelWriter::Write(int x, int y, const PixelColor &c) {
  auto p = PixelAt(x, y);
  p[0] = c.r;
  p[1] = c.g;
  p[2] = c.b;
}

void BGRResv8BitPixelWriter::Write(int x, int y, const PixelColor &c) {
  auto p = PixelAt(x, y);
  p[0] = c.b;
  p[1] = c.g;
  p[2] = c.r;
}

PixelWriter *pixel_writer;

namespace {
char pixel_writer_buf[sizeof(RGBResv8BitPixelWriter)];
}

void InitializeGraphics(const FrameBufferConfig &frame_buffer_config) {
  switch (frame_buffer_config.pixel_format) {
  case kPixelRGBResv8BitPerColor:
    pixel_writer = new (pixel_writer_buf) RGBResv8BitPixelWriter{frame_buffer_config};
    break;
  case kPixelBGRResv8BitPerColor:
    pixel_writer = new (pixel_writer_buf) BGRResv8BitPixelWriter{frame_buffer_config};
    break;
  }
}
