#pragma once

enum class DescriptorType {
  kTSSAvailable = 9,
  kInterruptGate = 14,

  kReadWrite = 2,
  kExecuteRead = 10,
};
