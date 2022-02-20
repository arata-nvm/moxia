#include "asmfunc.hpp"
#include "console.hpp"
#include "graphics.hpp"
#include "interrupt.hpp"
#include "keyboard.hpp"
#include "memory_manager.hpp"
#include "memory_map.hpp"
#include "message.hpp"
#include "paging.hpp"
#include "pic.hpp"
#include "printk.hpp"
#include "segment.hpp"
#include "task.hpp"
#include "terminal.hpp"
#include "timer.hpp"
#include <cstdint>
#include <deque>

alignas(16) uint8_t kernel_main_stack[1024 * 1024];

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

extern "C" void
KernelMainNewStack(const FrameBufferConfig &frame_buffer_config, const MemoryMap &memory_map) {
  InitializeGraphics(frame_buffer_config);
  InitializeConsole();

  printk("Hello, world!\n");

  InitializeSegmentation();
  InitializePaging();
  InitializeMemoryManager(memory_map);
  InitializePIC();
  InitializeInterrupt();

  InitializeLAPICTimer();

  InitializeTask();
  Task &terminal_task = task_manager->NewTask().InitContext(TaskTerminal, 0).Wakeup();

  Task &main_task = task_manager->CurrentTask();
  while (1) {
    __asm__("cli");
    auto msg = main_task.ReceiveMessage();
    if (!msg) {
      main_task.Sleep();
      __asm__("sti");
      continue;
    }
    __asm__("sti");

    switch (msg->type) {
    case Message::kTimerTimeout:
      printk("Timer: timeout = %lu, value = %d\n", msg->arg.timer.timeout, msg->arg.timer.value);
      if (msg->arg.timer.value > 0) {
        timer_manager->AddTimer(Timer(msg->arg.timer.timeout + 100, msg->arg.timer.value));
      }
      break;
    case Message::kKeyboardPush:
      task_manager->SendMessage(terminal_task.ID(), msg.value());
      break;
    }
  }
}
