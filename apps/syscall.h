#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SyscallResult {
  uint64_t value;
  int error;
};

struct SyscallResult SyscallRead(int fd, const void *buf, size_t len);
struct SyscallResult SyscallWrite(int fd, const void *buf, size_t len);
struct SyscallResult SyscallOpen(const char *path, int flags);
void SyscallExit(int exit_code);

#ifdef __cplusplus
}
#endif
