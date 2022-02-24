#include "terminal.hpp"
#include "console.hpp"
#include "elf.hpp"
#include "fat.hpp"
#include "keyboard.hpp"
#include "printk.hpp"
#include "task.hpp"
#include <string.h>
#include <string>

namespace {

std::vector<char *> MakeArgVector(char *cmd, char *arg) {
  std::vector<char *> argv;
  argv.push_back(cmd);

  char *p = arg;
  while (true) {
    while (isspace(p[0])) {
      ++p;
    }
    if (p[0] == 0) {
      break;
    }
    argv.push_back(p);

    while (p[0] != 0 && !isspace(p[0])) {
      ++p;
    }
    if (p[0] == 0) {
      break;
    }
    p[0] = 0;
    ++p;
  }

  return argv;
}

} // namespace

void ExecuteFile(const fat::DirectoryEntry &file_entry, char *cmd, char *arg) {
  uint32_t cluster = file_entry.FirstCluster();
  uint32_t remain_bytes = file_entry.file_size;

  std::vector<uint8_t> file_buf(remain_bytes);
  uint8_t *p = &file_buf[0];

  while (cluster != 0 && cluster != fat::kEndOfClusterchain) {
    const uint32_t copy_bytes = fat::bytes_per_cluster < remain_bytes ? fat::bytes_per_cluster : remain_bytes;
    memcpy(p, fat::GetSectorByCluster<uint8_t>(cluster), copy_bytes);

    remain_bytes -= copy_bytes;
    p += copy_bytes;
    cluster = fat::NextCluster(cluster);
  }

  Elf64_Ehdr *efl_header = reinterpret_cast<Elf64_Ehdr *>(&file_buf[0]);
  if (memcmp(efl_header->e_ident, "\x7f"
                                  "ELF",
             4) != 0) {
    using Func = void();
    Func *f = reinterpret_cast<Func *>(&file_buf[0]);
    f();
    return;
  }

  std::vector<char *> argv = MakeArgVector(cmd, arg);

  uintptr_t entry_addr = efl_header->e_entry;
  entry_addr += reinterpret_cast<uintptr_t>(&file_buf[0]);

  using Func = int(int, char **);
  Func *f = reinterpret_cast<Func *>(entry_addr);
  int ret = f(argv.size(), &argv[0]);

  printk("app exited. ret = %d\n", ret);
}

void ExecuteCommand(std::string line) {
  size_t i = line.find(' ');
  char *cmd, *arg;
  if (i == std::string::npos) {
    cmd = &line[0];
    arg = &line[line.size()];
  } else {
    line[i] = 0;
    cmd = &line[0];
    arg = &line[i + 1];
  }

  if (!strcmp(cmd, "echo")) {
    printk("%s\n", arg);
  } else if (!strcmp(cmd, "clear")) {
    console->Clear();
  } else if (!strcmp(cmd, "ls")) {
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
  } else if (!strcmp(cmd, "cat")) {
    fat::DirectoryEntry *file_entry = fat::FindFile(arg);
    if (!file_entry) {
      printk("no such file: %s\n", arg);
    } else {
      uint32_t cluster = file_entry->FirstCluster();
      uint32_t remain_bytes = file_entry->file_size;

      while (cluster != 0 && cluster != fat::kEndOfClusterchain) {
        char *p = fat::GetSectorByCluster<char>(cluster);

        int i = 0;
        for (; i < fat::bytes_per_cluster && i < remain_bytes; ++i) {
          printk("%c", *p);
          p++;
        }
        remain_bytes -= i;
        cluster = fat::NextCluster(cluster);
      }
    }
  } else {
    auto file_entry = fat::FindFile(cmd);
    if (!file_entry) {
      printk("command not found: %s\n", cmd);
    } else {
      ExecuteFile(*file_entry, cmd, arg);
    }
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
