#pragma once
#include "fat.hpp"
#include "file.hpp"
#include "task.hpp"
#include <stdint.h>
#include <string>

class TerminalFileDescriptor : public FileDescriptor {
public:
  explicit TerminalFileDescriptor(Task &task);
  size_t Read(void *buf, size_t len) override;
  size_t Write(const void *buf, size_t len) override;

private:
  Task &task_;
};

class PipeDescriptor : public FileDescriptor {
public:
  explicit PipeDescriptor(Task &task);
  size_t Read(void *buf, size_t len) override;
  size_t Write(const void *buf, size_t len) override;

  void FinishWrite();

private:
  Task &task_;
  char data_[16];
  size_t len_{0};
  bool closed_{false};
};

struct TerminalDescriptor {
  std::string command_line;
  std::array<std::shared_ptr<FileDescriptor>, 3> files;
};

void TaskTerminal(uint64_t task_id, int64_t data);
