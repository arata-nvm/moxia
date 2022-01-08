#include "console.hpp"
#include "graphics.hpp"
#include "interrupt.hpp"
#include "memory_manager.hpp"
#include "memory_map.hpp"
#include "message.hpp"
#include "paging.hpp"
#include "printk.hpp"
#include "segment.hpp"
#include "timer.hpp"
#include <cstdint>
#include <deque>

alignas(16) uint8_t kernel_main_stack[1024 * 1024];
std::deque<Message> *main_queue;

extern "C" void KernelMainNewStack(const FrameBufferConfig &frame_buffer_config, const MemoryMap &memory_map) {
  InitializeGraphics(frame_buffer_config);
  InitializeConsole();

  printk("Hello, world!\n");

  InitializeSegmentation();
  InitializePaging();
  InitializeMemoryManager(memory_map);
  InitializeInterrupt();

  ::main_queue = new std::deque<Message>();
  InitializeLAPICTimer(*main_queue);

  timer_manager->AddTimer(Timer(200, 2));
  timer_manager->AddTimer(Timer(600, -1));

  while (1) {
    __asm__("cli");
    if (main_queue->empty()) {
      __asm__("sti;"
              "hlt");
      continue;
    }

    Message msg = main_queue->front();
    main_queue->pop_front();
    __asm__("sti");

    switch (msg.type) {
    case Message::kTimerTimeout:
      printk("Timer: timeout = %lu, value = %d\n", msg.arg.timer.timeout, msg.arg.timer.value);
      if (msg.arg.timer.value > 0) {
        timer_manager->AddTimer(Timer(msg.arg.timer.timeout + 100, msg.arg.timer.value));
      }
      break;
    }
  }
}
