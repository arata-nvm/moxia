#pragma once
#include "fat.hpp"
#include "file.hpp"
#include "task.hpp"
#include <stdint.h>

class TerminalFileDescriptor : public FileDescriptor {
public:
  explicit TerminalFileDescriptor(Task &task);
  size_t Read(void *buf, size_t len) override;
  size_t Write(const void *buf, size_t len) override;

private:
  Task &task_;
};

void TaskTerminal(uint64_t task_id, int64_t data);
