#pragma once
#include "error.hpp"
#include "file.hpp"
#include <stddef.h>
#include <stdint.h>
#include <utility>

namespace fat {

static const uint32_t kEndOfClusterchain = 0x0ffffffflu;

struct BPB {
  uint8_t jump_boot[3];
  char oem_name[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sector_count;
  uint8_t num_fats;
  uint16_t root_entry_count;
  uint16_t total_sectors_16;
  uint8_t media;
  uint16_t fat_size_16;
  uint16_t sectors_per_track;
  uint16_t num_heads;
  uint32_t hidden_sectors;
  uint32_t total_sectors_32;
  uint32_t fat_size_32;
  uint16_t ext_flags;
  uint16_t fs_version;
  uint32_t root_cluster;
  uint16_t fs_info;
  uint16_t backup_boot_sector;
  uint8_t reserved[12];
  uint8_t drive_number;
  uint8_t reserved2;
  uint8_t boot_signature;
  uint32_t volume_id;
  char volume_label[11];
  char fs_type[8];
} __attribute__((packed));

enum class Attribute : uint8_t {
  kReadOnly = 0x01,
  kHiden = 0x02,
  kSystem = 0x04,
  kVolumeID = 0x08,
  kDirectory = 0x10,
  kArchive = 0x20,
  kLongName = 0x0f,
};

struct DirectoryEntry {
  unsigned char name[11];
  Attribute attr;
  uint8_t ntres;
  uint8_t create_time_tenth;
  uint16_t create_time;
  uint16_t create_date;
  uint16_t last_access_date;
  uint16_t first_cluter_high;
  uint16_t wirte_time;
  uint16_t write_date;
  uint16_t first_cluster_low;
  uint32_t file_size;

  uint32_t FirstCluster() const {
    return first_cluster_low | (static_cast<uint32_t>(first_cluter_high) << 16);
  }
} __attribute__((packed));

class FileDescriptor : public ::FileDescriptor {
public:
  explicit FileDescriptor(DirectoryEntry &fat_entry);
  size_t Read(void *buf, size_t len) override;
  size_t Write(const void *buf, size_t len) override;

private:
  DirectoryEntry &fat_entry_;

  size_t rd_off_ = 0;
  uint32_t rd_cluster_ = 0;
  size_t rd_cluster_off_ = 0;

  size_t wr_off_ = 0;
  uint32_t wr_cluster_ = 0;
  size_t wr_cluster_off_ = 0;
};

extern BPB *boot_volume_image;
extern uint32_t bytes_per_cluster;

void Initialize(void *volume_image);

uintptr_t GetClusterAddr(unsigned long cluster);

template <class T>
T *GetSectorByCluster(unsigned long cluster) {
  return reinterpret_cast<T *>(GetClusterAddr(cluster));
}

void ReadName(const DirectoryEntry &entry, char *base, char *ext);

uint32_t NextCluster(uint32_t cluster);

uint32_t *GetFAT();

bool IsEndOfClusterchain(uint32_t cluster);

uint32_t ExtendCluster(uint32_t eoc_cluster, size_t n);

DirectoryEntry *AllocateEntry(uint32_t dir_cluster);

void SetFileName(DirectoryEntry &entry, const char *name);

WithError<DirectoryEntry *> CreateFile(const char *path);

size_t LoadFile(void *buf, size_t len, const DirectoryEntry &entry);

std::pair<DirectoryEntry *, bool> FindFile(const char *path, uint32_t directory_cluster = 0);

bool NameIsEqual(const DirectoryEntry &entry, const char *name);

void FormatName(const DirectoryEntry &entry, char *dest);

uint32_t AllocateClusterchain(size_t n);

} // namespace fat
