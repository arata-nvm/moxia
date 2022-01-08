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
#include "timer.hpp"
#include <cstdint>
#include <deque>
#include <string.h>

struct TaskContext {
  uint64_t cr3, rip, rflags, reserved1;
  uint64_t cs, ss, fs, gs;
  uint64_t rax, rbx, rcx, rdx, rdi, rsi, rsp, rbp;
  uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
  std::array<uint8_t, 512> fxsave_area;
} __attribute__((packed));

alignas(16) TaskContext task_b_ctx, task_a_ctx;
alignas(16) uint8_t kernel_main_stack[1024 * 1024];
std::deque<Message> *main_queue;

void TaskB(int task_id, int data) {
  printk("TaskB: task_id=%d, data=%d\n", task_id, data);
  int count = 0;
  while (true) {
    ++count;
    printk("Task B: %010d\n", count);

    SwitchContext(&task_a_ctx, &task_b_ctx);
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
  InitializeInterrupt();

  ::main_queue = new std::deque<Message>();
  InitializeLAPICTimer(*main_queue);

  std::vector<uint64_t> task_b_stack(1024);
  uint64_t task_b_stack_end = reinterpret_cast<uint64_t>(&task_b_stack[1024]);

  memset(&task_b_ctx, 0, sizeof(task_b_ctx));
  task_b_ctx.rip = reinterpret_cast<uint64_t>(TaskB);
  task_b_ctx.rdi = 1;
  task_b_ctx.rsi = 42;

  task_b_ctx.cr3 = GetCR3();
  task_b_ctx.rflags = 0x202;
  task_b_ctx.cs = kKernelCS;
  task_b_ctx.ss = kKernelSS;
  task_b_ctx.rsp = (task_b_stack_end & ~0xflu) - 8;

  *reinterpret_cast<uint32_t *>(&task_b_ctx.fxsave_area[24]) = 0x1f80;

  while (1) {
    __asm__("cli");
    const auto tick = timer_manager->CurrentTick();
    __asm__("sti");

    printk("Task A: %ul\n", tick);

    __asm__("cli");
    if (main_queue->empty()) {
      __asm__("sti");
      SwitchContext(&task_b_ctx, &task_a_ctx);
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
