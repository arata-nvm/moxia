#pragma once

#include "error.hpp"
#include "fat.hpp"
#include "file.hpp"
#include "message.hpp"
#include <array>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <vector>

struct TaskContext {
  uint64_t cr3, rip, rflags, reserved1;
  uint64_t cs, ss, fs, gs;
  uint64_t rax, rbx, rcx, rdx, rdi, rsi, rsp, rbp;
  uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
  std::array<uint8_t, 512> fxsave_area;
} __attribute__((packed));

using TaskFunc = void(uint64_t, int64_t);

class TaskManager;

class Task {
public:
  static const size_t kDefaultStackBytes = 4096;
  static const unsigned int kDefaultLevel = 1;

  Task(uint64_t id);
  Task &InitContext(TaskFunc *, int64_t data);
  TaskContext &Context();

  uint64_t ID() const;
  unsigned int Level() const;
  uint64_t &OSStackPointer();
  std::vector<std::shared_ptr<::FileDescriptor>> &Files();

  bool Running() const;
  Task &Sleep();
  Task &Wakeup();

  void SendMessage(const Message &msg);
  std::optional<Message> ReceiveMessage();

  size_t AllocateFD();

private:
  uint64_t id_;
  std::vector<uint64_t> stack_;
  alignas(16) TaskContext context_;
  std::deque<Message> msgs_;
  unsigned int level_{kDefaultLevel};
  bool running_{false};
  uint64_t os_stack_ptr_;
  std::vector<std::shared_ptr<::FileDescriptor>> files_{};

  Task &SetLevel(int level) {
    level_ = level;
    return *this;
  }
  Task &SetRunning(bool running) {
    running_ = running;
    return *this;
  }

  friend TaskManager;
};

class TaskManager {
public:
  static const int kMaxLevel = 3;

  TaskManager();
  Task &NewTask();
  void SwitchTask(const TaskContext &current_ctx);
  Task &CurrentTask();

  void Sleep(Task *task);
  Error Sleep(uint64_t id);
  void Wakeup(Task *task, int level = -1);
  Error Wakeup(uint64_t id, int level = -1);

  Error SendMessage(uint64_t id, const Message &msg);

  void Finish(int exit_code);
  WithError<int> WaitFinish(uint64_t task_id);

private:
  std::vector<std::unique_ptr<Task>> tasks_{};
  uint64_t latest_id_{0};
  std::array<std::deque<Task *>, kMaxLevel + 1> running_{};
  int current_level_{kMaxLevel};
  bool level_changed_{false};
  std::map<uint64_t, int> finish_tasks_{};
  std::map<uint64_t, Task *> finish_waiter_{};

  void ChangeLevelRunning(Task *task, int level);
  Task *RotateCurrentRunQueue(bool current_sleep);
};

extern TaskManager *task_manager;

void InitializeTask();
