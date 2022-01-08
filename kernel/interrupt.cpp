#include "interrupt.hpp"
#include "asmfunc.hpp"
#include "printk.hpp"
#include "segment.hpp"
#include <array>

std::array<InterruptDescriptor, 256> idt;

namespace {
__attribute__((interrupt)) void IntHandlerLAPICTimer(InterruptFrame *frame) {
  printk("Timer interrupt\n");
  NotifyEndOfInterrupt();
}
} // namespace

constexpr InterruptDescriptorAttribute MakeIDTAttr(DescriptorType type, uint8_t descriptor_privilege_level, bool present = true, uint8_t interrupt_stack_table = 0) {
  InterruptDescriptorAttribute attr{};
  attr.bits.interrupt_stack_table = interrupt_stack_table;
  attr.bits.type = type;
  attr.bits.descriptor_privilege_level = descriptor_privilege_level;
  attr.bits.present = present;
  return attr;
}

void SetIDTEntry(InterruptDescriptor &desc, InterruptDescriptorAttribute attr, uint64_t offset, uint16_t segment_selector) {
  desc.attr = attr;
  desc.offset_low = offset & 0xffffu;
  desc.offset_middle = (offset >> 16) & 0xffffu;
  desc.offset_high = offset >> 32;
  desc.segment_selector = segment_selector;
}

void NotifyEndOfInterrupt() {
  volatile auto end_of_interrupt = reinterpret_cast<uint32_t *>(0xfee000b0);
  *end_of_interrupt = 0;
}

void InitializeInterrupt() {
  SetIDTEntry(idt[InterruptVector::kLAPICTimer], MakeIDTAttr(DescriptorType::kInterruptGate, 0), reinterpret_cast<uint64_t>(IntHandlerLAPICTimer), kKernelCS);
  LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));
}
