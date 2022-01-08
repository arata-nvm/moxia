#include "console.hpp"
#include "graphics.hpp"
#include "interrupt.hpp"
#include "memory_manager.hpp"
#include "memory_map.hpp"
#include "paging.hpp"
#include "printk.hpp"
#include "segment.hpp"
#include "timer.hpp"
#include <cstdint>

alignas(16) uint8_t kernel_main_stack[1024 * 1024];

extern "C" void KernelMainNewStack(const FrameBufferConfig &frame_buffer_config, const MemoryMap &memory_map) {
  InitializeGraphics(frame_buffer_config);
  InitializeConsole();

  printk("Hello, world!\n");

  InitializeSegmentation();
  InitializePaging();
  InitializeMemoryManager(memory_map);
  InitializeInterrupt();

  InitializeLAPICTimer();

  __asm__("sti");

  while (1)
    __asm__("hlt");
}
