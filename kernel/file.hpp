#pragma once

#include <cstddef>

class FileDescriptor {
public:
  virtual ~FileDescriptor() = default;
  virtual size_t Read(void *buf, size_t len) = 0;
  virtual size_t Write(const void *buf, size_t len) = 0;
};

size_t PrintToFD(FileDescriptor &fd, const char *format, ...);
