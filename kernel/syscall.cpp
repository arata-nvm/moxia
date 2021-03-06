#include "asmfunc.hpp"
#include "console.hpp"
#include "fat.hpp"
#include "msr.hpp"
#include "printk.hpp"
#include "segment.hpp"
#include "task.hpp"
#include <array>
#include <cerrno>
#include <fcntl.h>
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

SYSCALL(read) {
  const int fd = arg1;
  void *buf = reinterpret_cast<void *>(arg2);
  size_t count = arg3;
  __asm__("cli");
  auto &task = task_manager->CurrentTask();
  __asm__("sti");

  if (fd < 0 || task.Files().size() <= fd || !task.Files()[fd]) {
    return {0, EBADF};
  }

  return {task.Files()[fd]->Read(buf, count), 0};
}

// TODO: why can't I use printk() here ?
SYSCALL(write) {
  const auto fd = arg1;
  const char *buf = reinterpret_cast<const char *>(arg2);
  const auto count = arg3;
  if (count > 1024) {
    return {0, E2BIG};
  }

  __asm__("cli");
  auto &task = task_manager->CurrentTask();
  __asm__("sti");

  if (fd < 0 || task.Files().size() <= fd || !task.Files()[fd]) {
    return {0, EBADF};
  }

  return {task.Files()[fd]->Write(buf, count), 0};
}

SYSCALL(open) {
  const char *path = reinterpret_cast<const char *>(arg1);
  const int flags = arg2;
  __asm__("cli");
  auto &task = task_manager->CurrentTask();
  __asm__("sti");

  if (strcmp(path, "@stdin") == 0) {
    return {0, 0};
  }

  auto [file, post_slash] = fat::FindFile(path);
  if (file == nullptr) {
    if ((flags & O_ACCMODE) == 0) {
      return {0, ENOENT};
    }

    auto [new_file, err] = fat::CreateFile(path);
    switch (err.Cause()) {
    case Error::kIsDirectory:
      return {0, EISDIR};
    case Error::kNoSuchDirectory:
      return {0, ENOENT};
    case Error::kNoEnoughMemory:
      return {0, ENOSPC};
    }

    file = new_file;
  } else if (file->attr != fat::Attribute::kDirectory && post_slash) {
    return {0, ENOENT};
  }

  size_t fd = task.AllocateFD();
  task.Files()[fd] = std::make_unique<fat::FileDescriptor>(*file);
  return {fd, 0};
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

extern "C" std::array<SyscallFuncType *, 4> syscall_table{
    /* 0x00 */ syscall::read,
    /* 0x01 */ syscall::write,
    /* 0x02 */ syscall::open,
    /* 0x03 */ syscall::exit,
};

void InitializeSyscall() {
  WriteMSR(kIA32_EFER, 0x0501u);
  WriteMSR(kIA32_LSTAR, reinterpret_cast<uint64_t>(SyscallEntry));
  WriteMSR(kIA32_STAR, static_cast<uint64_t>(kKernelCS) << 32 | static_cast<uint64_t>(kKernelSS | 3) << 48);
  WriteMSR(kIA32_FMASK, 0);
}
