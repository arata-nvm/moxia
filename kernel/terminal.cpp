#include "terminal.hpp"
#include "asmfunc.hpp"
#include "console.hpp"
#include "elf.hpp"
#include "fat.hpp"
#include "file.hpp"
#include "keyboard.hpp"
#include "memory_manager.hpp"
#include "paging.hpp"
#include "printk.hpp"
#include "segment.hpp"
#include "task.hpp"
#include <string.h>
#include <string>

namespace {

std::array<std::shared_ptr<FileDescriptor>, 3> files;

WithError<int> MakeArgVector(char *cmd, char *first_arg, char **argv, int argv_len, char *argbuf, int argbuf_len) {
  int argc = 0;
  int argbuf_index = 0;

  auto push_to_argv = [&](const char *s) {
    if (argc >= argv_len || argbuf_index >= argbuf_len) {
      return MAKE_ERROR(Error::kFull);
    }

    argv[argc] = &argbuf[argbuf_index];
    ++argc;
    strcpy(&argbuf[argbuf_index], s);
    argbuf_index += strlen(s) + 1;
    return MAKE_ERROR(Error::kSuccess);
  };

  if (auto err = push_to_argv(cmd)) {
    return {argc, err};
  }
  if (!first_arg) {
    return {argc, MAKE_ERROR(Error::kSuccess)};
  }

  char *p = first_arg;
  while (true) {
    while (isspace(p[0])) {
      ++p;
    }
    if (p[0] == 0) {
      break;
    }
    const char *arg = p;

    while (p[0] != 0 && !isspace(p[0])) {
      ++p;
    }
    const bool is_end = p[0] == 0;
    p[0] = 0;
    if (auto err = push_to_argv(arg)) {
      return {argc, err};
    }
    if (is_end) {
      break;
    }
    ++p;
  }

  return {argc, MAKE_ERROR(Error::kSuccess)};
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
    page_map[entry_index].bits.user = 1;

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

WithError<PageMapEntry *> SetupPML4(Task &current_task) {
  auto pml4 = NewPageMap();
  if (pml4.error) {
    return pml4;
  }

  const auto current_pml4 = reinterpret_cast<PageMapEntry *>(GetCR3());
  memcpy(pml4.value, current_pml4, 256 * sizeof(uint64_t));

  const auto cr3 = reinterpret_cast<uint64_t>(pml4.value);
  SetCR3(cr3);
  current_task.Context().cr3 = cr3;
  return pml4;
}

Error FreePML4(Task &current_task) {
  const auto cr3 = current_task.Context().cr3;
  current_task.Context().cr3 = 0;
  ResetCR3();

  const FrameID frame{cr3 / kBytesPerFrame};
  return memory_manager->Free(frame, 1);
}

void ListAllEntries(uint32_t dir_cluster) {
  const auto kEntriesPerCluster = fat::bytes_per_cluster / sizeof(fat::DirectoryEntry);

  while (dir_cluster != fat::kEndOfClusterchain) {
    auto dir = fat::GetSectorByCluster<fat::DirectoryEntry>(dir_cluster);

    for (int i = 0; i < kEntriesPerCluster; ++i) {
      if (dir[i].name[0] == 0x00) {
        return;
      } else if (static_cast<uint8_t>(dir[i].name[0]) == 0xe5) {
        continue;
      } else if (dir[i].attr == fat::Attribute::kLongName) {
        continue;
      }

      char name[13];
      fat::FormatName(dir[i], name);
      printk("%s\n", name);
    }

    dir_cluster = fat::NextCluster(dir_cluster);
  }
}

} // namespace

TerminalFileDescriptor::TerminalFileDescriptor(Task &task) : task_{task} {}

size_t TerminalFileDescriptor::Read(void *buf, size_t len) {
  char *bufc = reinterpret_cast<char *>(buf);

  while (true) {
    __asm__("cli");
    auto msg = task_.ReceiveMessage();
    if (!msg) {
      // TODO: why can"t I Sleep here?
      // task_.Sleep();
      __asm__("sti");
      continue;
    }
    __asm__("sti");

    if (msg->type != Message::kKeyboardPush) {
      continue;
    }

    char c = msg->arg.keyboard.keycode & kKeyCharMask;

    if (msg->arg.keyboard.keycode & kKeyCtrlMask) {
      char s[3] = "^ ";
      s[1] = toupper(c);
      console->PutString(s);
      if (c == 'd') {
        return 0; // EOT
      }
      continue;
    }

    bufc[0] = c;
    console->PutChar(c);
    return 1;
  }
}

size_t TerminalFileDescriptor::Write(const void *buf, size_t len) {
  for (size_t i = 0; i < len; i++) {
    console->PutChar(reinterpret_cast<const char *>(buf)[i]);
  }
  return len;
}

Error ExecuteFile(const fat::DirectoryEntry &file_entry, char *cmd, char *first_arg) {
  std::vector<uint8_t> file_buf(file_entry.file_size);
  fat::LoadFile(&file_buf[0], file_buf.size(), file_entry);

  Elf64_Ehdr *efl_header = reinterpret_cast<Elf64_Ehdr *>(&file_buf[0]);
  if (memcmp(efl_header->e_ident, "\x7f"
                                  "ELF",
             4) != 0) {
    return MAKE_ERROR(Error::kInvalidFormat);
  }

  __asm__("cli");
  auto &task = task_manager->CurrentTask();
  __asm__("sti");

  if (auto pml4 = SetupPML4(task); pml4.error) {
    return pml4.error;
  }

  if (auto err = LoadELF(efl_header)) {
    return err;
  }

  LinearAddress4Level args_frame_addr{0xffff'ffff'ffff'f000};
  if (auto err = SetupPageMaps(args_frame_addr, 1)) {
    return err;
  }
  auto argv = reinterpret_cast<char **>(args_frame_addr.value);
  int argv_len = 32;
  auto argbuf = reinterpret_cast<char *>(args_frame_addr.value + sizeof(char **) * argv_len);
  int argbuf_len = 4096 - sizeof(char **) * argv_len;
  auto argc = MakeArgVector(cmd, first_arg, argv, argv_len, argbuf, argbuf_len);
  if (argc.error) {
    return argc.error;
  }

  LinearAddress4Level stack_frame_addr{0xffff'ffff'ffff'e000};
  if (auto err = SetupPageMaps(stack_frame_addr, 1)) {
    return err;
  }

  for (int i = 0; i < files.size(); ++i) {
    task.Files().push_back(files[i]);
  }

  auto entry_addr = efl_header->e_entry;
  int ret = CallApp(argc.value, argv, kUserSS | 3, entry_addr, stack_frame_addr.value + 4096 - 8, &task.OSStackPointer());

  task.Files().clear();

  printk("app exited. ret = %d\n", ret);

  const auto addr_first = GetFirstLoadAddress(efl_header);
  if (auto err = CleanPageMaps(LinearAddress4Level{addr_first})) {
    return err;
  }

  return FreePML4(task);
}

void ExecuteCommand(std::string line) {
  char *cmd = &line[0];
  char *arg = strchr(&line[0], ' ');
  char *redir_char = strchr(&line[0], '>');
  if (arg) {
    *arg = 0;
    ++arg;
  }

  auto original_stdout = files[1];

  if (redir_char) {
    *redir_char = 0;
    char *redir_dest = &redir_char[1];
    while (isspace(*redir_dest)) {
      ++redir_dest;
    }

    auto [file, post_slash] = fat::FindFile(redir_dest);
    if (file == nullptr) {
      auto [new_file, err] = fat::CreateFile(redir_dest);
      if (err) {
        PrintToFD(*files[2], "failed to create a redirect file: %s\n", err.Name());
        return;
      }
      file = new_file;
    } else if (file->attr == fat::Attribute::kDirectory || post_slash) {
      PrintToFD(*files[2], "cannot redirect to a directory\n");
      return;
    }
    files[1] = std::make_shared<fat::FileDescriptor>(*file);
  }

  if (!strcmp(cmd, "echo")) {
    PrintToFD(*files[1], "%s\n", arg);
  } else if (!strcmp(cmd, "clear")) {
    console->Clear();
  } else if (!strcmp(cmd, "ls")) {
    if (arg[0] == '\0') {
      ListAllEntries(fat::boot_volume_image->root_cluster);
    } else {
      auto [dir, post_slash] = fat::FindFile(arg);
      if (dir == nullptr) {
        PrintToFD(*files[2], "Not such file or directory: %s\n", arg);
      } else if (dir->attr == fat::Attribute::kDirectory) {
        ListAllEntries(dir->FirstCluster());
      } else {
        char name[13];
        fat::FormatName(*dir, name);
        if (post_slash) {
          PrintToFD(*files[2], "%s is not a directory\n", name);
        } else {
          PrintToFD(*files[1], "%s\n");
        }
      }
    }

  } else if (!strcmp(cmd, "cat")) {
    auto [file_entry, post_slash] = fat::FindFile(arg);
    if (!file_entry) {
      PrintToFD(*files[2], "no such file: %s\n", arg);
    } else {
      uint32_t cluster = file_entry->FirstCluster();
      uint32_t remain_bytes = file_entry->file_size;

      while (cluster != 0 && cluster != fat::kEndOfClusterchain) {
        char *p = fat::GetSectorByCluster<char>(cluster);

        int i = 0;
        for (; i < fat::bytes_per_cluster && i < remain_bytes; ++i) {
          PrintToFD(*files[1], "%c", *p);
          p++;
        }
        remain_bytes -= i;
        cluster = fat::NextCluster(cluster);
      }
    }
  } else {
    auto [file_entry, post_slash] = fat::FindFile(cmd);
    if (!file_entry) {
      printk("command not found: %s\n", cmd);
    } else if (file_entry->attr != fat::Attribute::kDirectory && post_slash) {
      char name[13];
      fat::FormatName(*file_entry, name);
      printk("%s is not a directory\n", name);
    } else if (auto err = ExecuteFile(*file_entry, cmd, arg)) {
      printk("failed to exec file: %s\n", err.Name());
    }
  }

  files[1] = original_stdout;
}

void TaskTerminal(uint64_t task_id, int64_t data) {
  __asm__("cli");
  Task &task = task_manager->CurrentTask();
  __asm__("sti");

  for (int i = 0; i < files.size(); ++i) {
    files[i] = std::make_shared<TerminalFileDescriptor>(task);
  }

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
