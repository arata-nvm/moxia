#include "terminal.hpp"
#include "keyboard.hpp"
#include "printk.hpp"
#include "task.hpp"
#include <string>

void ExecuteCommand(std::string line) {
  size_t i = line.find(' ');
  std::string cmd = line.substr(0, i);
  std::string arg;
  if (i != std::string::npos) {
    arg = line.substr(i + 1);
  } else {
    arg = "";
  }

  if (cmd == "echo") {
    printk("%s\n", arg.c_str());
  } else {
    printk("command not found: %s\n", cmd.c_str());
  }
}

void TaskTerminal(uint64_t task_id, int64_t data) {
  __asm__("cli");
  Task &task = task_manager->CurrentTask();
  __asm__("sti");

  std::string line_buf;
  printk("> ");

  while (true) {
    __asm__("cli");
    auto msg = task.ReceiveMessage();
    if (!msg) {
      task.Sleep();
      __asm__("sti");
      continue;
    }
    __asm__("sti");

    if (msg->type == Message::kKeyboardPush) {
      char c = msg->arg.keyboard.keycode & kKeyCharMask;
      printk("%c", c);

      if (c == '\n') {
        ExecuteCommand(line_buf);
        line_buf.clear();
        printk("> ");
      } else {
        line_buf.push_back(c);
      }
    }
  }
}
