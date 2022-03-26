#include "asmfunc.hpp"
#include "console.hpp"
#include "msr.hpp"
#include "printk.hpp"
#include "segment.hpp"
#include "task.hpp"
#include <array>
#include <cerrno>
#include <stdint.h>
#include <string.h>

namespace syscall {

struct Result {
  uint64_t value;
  int error;
};

#define SYSCALL(name)                              \
  Result name(                                     \
      uint64_t arg1, uint64_t arg2, uint64_t arg3, \
      uint64_t arg4, uint64_t arg5, uint64_t arg6)

// TODO: why can't I use printk() here ?
SYSCALL(write) {
  const auto fd = arg1;
  const char *s = reinterpret_cast<const char *>(arg2);
  const auto len = arg3;
  if (len > 1024) {
    return {0, E2BIG};
  }

  if (fd == 1) {
    for (size_t i = 0; i < len; i++) {
      console->PutChar(s[i]);
    }
    return {len, 0};
  }
  return {0, EBADF};
}

SYSCALL(exit) {
  __asm__("cli");
  auto &task = task_manager->CurrentTask();
  __asm__("sti");
  return {task.OSStackPointer(), static_cast<int>(arg1)};
}

} // namespace syscall

using SyscallFuncType = syscall::Result(uint64_t, uint64_t, uint64_t,
                                        uint64_t, uint64_t, uint64_t);

extern "C" std::array<SyscallFuncType *, 2> syscall_table{
    /* 0x00 */ syscall::write,
    /* 0x01 */ syscall::exit,
};

void InitializeSyscall() {
  WriteMSR(kIA32_EFER, 0x0501u);
  WriteMSR(kIA32_LSTAR, reinterpret_cast<uint64_t>(SyscallEntry));
  WriteMSR(kIA32_STAR, static_cast<uint64_t>(kKernelCS) << 32 | static_cast<uint64_t>(kKernelSS | 3) << 48);
  WriteMSR(kIA32_FMASK, 0);
}
