#include "asmfunc.hpp"
#include "console.hpp"
#include "fonts.hpp"
#include "graphics.hpp"
#include "memory_manager.hpp"
#include "memory_map.hpp"
#include "paging.hpp"
#include "segment.hpp"
#include <cstdint>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

char pixel_writer_buf[sizeof(RGBResv8BitPixelWriter)];
PixelWriter *pixel_writer;

char console_buf[sizeof(Console)];
Console *console;

char memory_manager_buf[sizeof(BitmapMemoryManager)];
BitmapMemoryManager *memory_manager;

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

  ::memory_manager = new (memory_manager_buf) BitmapMemoryManager();

  const auto memory_map_base = reinterpret_cast<uintptr_t>(memory_map.buffer);
  uintptr_t available_end = 0;
  for (uintptr_t iter = memory_map_base; iter < memory_map_base + memory_map.map_size; iter += memory_map.descriptor_size) {
    auto desc = reinterpret_cast<const MemoryDescriptor *>(iter);
    if (available_end < desc->physical_start) {
      memory_manager->MarkAllocated(FrameID{available_end / kBytesPerFrame}, (desc->physical_start - available_end) / kBytesPerFrame);
    }

    const auto physical_end = desc->physical_start + desc->number_of_pages * kUEFIPageSize;
    if (IsAvailable(static_cast<MemoryType>(desc->type))) {
      available_end = physical_end;
    } else {
      memory_manager->MarkAllocated(FrameID{desc->physical_start / kBytesPerFrame}, desc->number_of_pages * kUEFIPageSize / kBytesPerFrame);
    }
  }
  memory_manager->SetMemoryRange(FrameID{1}, FrameID{available_end});

  while (1)
    __asm__("hlt");
}
