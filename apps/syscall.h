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

struct SyscallResult SyscallWrite(int fd, const void *buf, size_t len);
void SyscallExit(int exit_code);

#ifdef __cplusplus
}
#endif
