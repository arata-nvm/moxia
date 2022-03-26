#include "terminal.hpp"
#include "asmfunc.hpp"
#include "console.hpp"
#include "elf.hpp"
#include "fat.hpp"
#include "keyboard.hpp"
#include "memory_manager.hpp"
#include "paging.hpp"
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

Elf64_Phdr *GetProgramHeader(Elf64_Ehdr *ehdr) {
  return reinterpret_cast<Elf64_Phdr *>(reinterpret_cast<uintptr_t>(ehdr) + ehdr->e_phoff);
}

uintptr_t GetFirstLoadAddress(Elf64_Ehdr *ehdr) {
  auto phdr = GetProgramHeader(ehdr);
  for (int i = 0; i < ehdr->e_phnum; ++i) {
    if (phdr[i].p_type != PT_LOAD)
      continue;
    return phdr[i].p_vaddr;
  }
  return 0;
}

WithError<PageMapEntry *> NewPageMap() {
  auto frame = memory_manager->Allocate(1);
  if (frame.error) {
    return {nullptr, frame.error};
  }

  auto e = reinterpret_cast<PageMapEntry *>(frame.value.Frame());
  memset(e, 0, sizeof(uint64_t) * 512);
  return {e, MAKE_ERROR(Error::kSuccess)};
}

WithError<PageMapEntry *> SetNewPageMapIfNotPresent(PageMapEntry &entry) {
  if (entry.bits.present) {
    return {entry.Pointer(), MAKE_ERROR(Error::kSuccess)};
  }

  auto [child_map, err] = NewPageMap();
  if (err) {
    return {nullptr, err};
  }

  entry.SetPointer(child_map);
  entry.bits.present = 1;

  return {child_map, MAKE_ERROR(Error::kSuccess)};
}

WithError<size_t> SetupPageMap(PageMapEntry *page_map, int page_map_level, LinearAddress4Level addr, size_t num_4kpages) {
  while (num_4kpages > 0) {
    const auto entry_index = addr.Part(page_map_level);

    auto [child_map, err] = SetNewPageMapIfNotPresent(page_map[entry_index]);
    if (err) {
      return {num_4kpages, err};
    }
    page_map[entry_index].bits.writable = 1;

    if (page_map_level == 1) {
      --num_4kpages;
    } else {
      auto [num_remain_pages, err] = SetupPageMap(child_map, page_map_level - 1, addr, num_4kpages);
      if (err) {
        return {num_4kpages, err};
      }
      num_4kpages = num_remain_pages;
    }

    if (entry_index == 511) {
      break;
    }

    addr.SetPart(page_map_level, entry_index + 1);
    for (int level = page_map_level - 1; level >= 1; --level) {
      addr.SetPart(level, 0);
    }
  }

  return {num_4kpages, MAKE_ERROR(Error::kSuccess)};
}

Error SetupPageMaps(LinearAddress4Level addr, size_t num_4kpages) {
  auto pml4_table = reinterpret_cast<PageMapEntry *>(GetCR3());
  return SetupPageMap(pml4_table, 4, addr, num_4kpages).error;
}

Error CleanPageMap(PageMapEntry *page_map, int page_map_level) {
  for (int i = 0; i < 512; ++i) {
    auto entry = page_map[i];
    if (!entry.bits.present) {
      continue;
    }

    if (page_map_level > 1) {
      if (auto err = CleanPageMap(entry.Pointer(), page_map_level - 1)) {
        return err;
      }
    }

    const auto entry_addr = reinterpret_cast<uintptr_t>(entry.Pointer());
    const FrameID map_frame{entry_addr / kBytesPerFrame};
    if (auto err = memory_manager->Free(map_frame, 1)) {
      return err;
    }
    page_map[i].data = 0;
  }

  return MAKE_ERROR(Error::kSuccess);
}

Error CleanPageMaps(LinearAddress4Level addr) {
  auto pml4_table = reinterpret_cast<PageMapEntry *>(GetCR3());
  auto pdp_table = pml4_table[addr.parts.pml4].Pointer();
  pml4_table[addr.parts.pml4].data = 0;
  if (auto err = CleanPageMap(pdp_table, 3)) {
    return err;
  }

  const auto pdp_addr = reinterpret_cast<uintptr_t>(pdp_table);
  const FrameID pdp_frame{pdp_addr / kBytesPerFrame};
  return memory_manager->Free(pdp_frame, 1);
}

Error CopyLoadSegments(Elf64_Ehdr *ehdr) {
  auto phdr = GetProgramHeader(ehdr);
  for (int i = 0; i < ehdr->e_phnum; ++i) {
    if (phdr[i].p_type != PT_LOAD)
      continue;

    LinearAddress4Level dest_addr;
    dest_addr.value = phdr[i].p_vaddr;
    const auto num_4kpages = (phdr[i].p_memsz + 4095) / 4096;

    if (auto err = SetupPageMaps(dest_addr, num_4kpages)) {
      return err;
    }

    const auto src = reinterpret_cast<uint8_t *>(ehdr) + phdr[i].p_offset;
    const auto dst = reinterpret_cast<uint8_t *>(phdr[i].p_vaddr);
    memcpy(dst, src, phdr[i].p_filesz);
    memset(dst + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
  }
  return MAKE_ERROR(Error::kSuccess);
}

Error LoadELF(Elf64_Ehdr *ehdr) {
  if (ehdr->e_type != ET_EXEC) {
    return MAKE_ERROR(Error::kInvalidFormat);
  }

  const auto addr_first = GetFirstLoadAddress(ehdr);
  if (addr_first < 0xffff'8000'0000'0000) {
    return MAKE_ERROR(Error::kInvalidFormat);
  }

  if (auto err = CopyLoadSegments(ehdr)) {
    return err;
  }

  return MAKE_ERROR(Error::kSuccess);
}

} // namespace

Error ExecuteFile(const fat::DirectoryEntry &file_entry, char *cmd, char *arg) {
  std::vector<uint8_t> file_buf(file_entry.file_size);
  fat::LoadFile(&file_buf[0], file_buf.size(), file_entry);

  Elf64_Ehdr *efl_header = reinterpret_cast<Elf64_Ehdr *>(&file_buf[0]);
  if (memcmp(efl_header->e_ident, "\x7f"
                                  "ELF",
             4) != 0) {
    using Func = void();
    Func *f = reinterpret_cast<Func *>(&file_buf[0]);
    f();
    return MAKE_ERROR(Error::kSuccess);
  }

  std::vector<char *> argv = MakeArgVector(cmd, arg);
  if (auto err = LoadELF(efl_header)) {
    return err;
  }

  auto entry_addr = efl_header->e_entry;
  using Func = int(int, char **);
  auto f = reinterpret_cast<Func *>(entry_addr);
  auto ret = f(argv.size(), &argv[0]);

  printk("app exited. ret = %d\n", ret);

  const auto addr_first = GetFirstLoadAddress(efl_header);
  if (auto err = CleanPageMaps(LinearAddress4Level{addr_first})) {
    return err;
  }

  return MAKE_ERROR(Error::kSuccess);
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
    } else if (auto err = ExecuteFile(*file_entry, cmd, arg)) {
      printk("failed to exec file: %s\n", err.Name());
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
