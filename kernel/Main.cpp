#include "asmfunc.hpp"
#include "console.hpp"
#include "graphics.hpp"
#include "interrupt.hpp"
#include "memory_manager.hpp"
#include "memory_map.hpp"
#include "message.hpp"
#include "paging.hpp"
#include "printk.hpp"
#include "segment.hpp"
#include "task.hpp"
#include "timer.hpp"
#include <cstdint>
#include <deque>
#include <string.h>

alignas(16) uint8_t kernel_main_stack[1024 * 1024];
std::deque<Message> *main_queue;

uint64_t count_a, count_b;

void TaskB(uint64_t task_id, int64_t data) {
  printk("TaskB: task_id=%lu, data=%lx\n", task_id, data);
  while (true) {
    __asm__("cli");
    ++count_b;
    printk("TaskA: %010u, TaskB: %010u\r", count_a, count_b);
    __asm__("sti");
  }
}

void TaskIdle(uint64_t task_id, int64_t data) {
  printk("TaskIdle: task_id=%lu, data=%lx\n", task_id, data);
  while (true)
    __asm__("hlt");
}

extern "C" void
KernelMainNewStack(const FrameBufferConfig &frame_buffer_config, const MemoryMap &memory_map) {
  InitializeGraphics(frame_buffer_config);
  InitializeConsole();

  printk("Hello, world!\n");

  InitializeSegmentation();
  InitializePaging();
  InitializeMemoryManager(memory_map);
  InitializeInterrupt();

  ::main_queue = new std::deque<Message>();
  InitializeLAPICTimer(*main_queue);

  InitializeTask();
  task_manager->NewTask().InitContext(TaskB, 0).Wakeup();
  task_manager->NewTask().InitContext(TaskIdle, 1);
  task_manager->NewTask().InitContext(TaskIdle, 2);

  while (1) {
    __asm__("cli");
    ++count_a;
    printk("TaskA: %010u, TaskB: %010u\r", count_a, count_b);
    __asm__("sti");

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
