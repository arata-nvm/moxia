#include "terminal.hpp"
#include "console.hpp"
#include "fat.hpp"
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
  } else if (cmd == "clear") {
    console->Clear();
  } else if (cmd == "ls") {
    fat::DirectoryEntry *root_dir_entries = fat::GetSectorByCluster<fat::DirectoryEntry>(fat::boot_volume_image->root_cluster);
    int entries_per_cluster = fat::boot_volume_image->bytes_per_sector / sizeof(fat::DirectoryEntry) * fat::boot_volume_image->sectors_per_cluster;
    char base[9], ext[4];
    char s[64];
    for (int i = 0; i < entries_per_cluster; ++i) {
      fat::ReadName(root_dir_entries[i], base, ext);
      if (base[0] == 0x00) {
        break;
      } else if (static_cast<uint8_t>(base[0]) == 0xe5) {
        continue;
      } else if (root_dir_entries[i].attr == fat::Attribute::kLongName) {
        continue;
      }

      if (ext[0]) {
        sprintf(s, "%s.%s\n", base, ext);
      } else {
        sprintf(s, "%s\n", base);
      }
      printk("%s", s);
    }
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
