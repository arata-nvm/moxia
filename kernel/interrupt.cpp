#include "interrupt.hpp"
#include "asmfunc.hpp"
#include "keyboard.hpp"
#include "pic.hpp"
#include "printk.hpp"
#include "segment.hpp"
#include "timer.hpp"
#include <array>

std::array<InterruptDescriptor, 256> idt;
namespace {
void PrintFrame(InterruptFrame *frame, const char *exp_name) {
  printk("%s\n", exp_name);
  printk("CS: %x\n", frame->cs);
  printk("RIP: %x\n", frame->rip);
  printk("RFLAGS: %x\n", frame->rflags);
  printk("SS: %x\n", frame->ss);
  printk("RSP: %x\n", frame->rsp);
}

#define FaultHandlerWithError(fault_name)                                                              \
  __attribute__((interrupt)) void IntHandler##fault_name(InterruptFrame *frame, uint64_t error_code) { \
    PrintFrame(frame, "#" #fault_name);                                                                \
    printk("[ERR] %x\n", error_code);                                                                  \
    while (true)                                                                                       \
      __asm__("hlt");                                                                                  \
  }

#define FaultHandlerWithNoError(fault_name)                                       \
  __attribute__((interrupt)) void IntHandler##fault_name(InterruptFrame *frame) { \
    PrintFrame(frame, "#" #fault_name);                                           \
    while (true)                                                                  \
      __asm__("hlt");                                                             \
  }

FaultHandlerWithNoError(DE);
FaultHandlerWithNoError(DB);
FaultHandlerWithNoError(DP);
FaultHandlerWithNoError(OF);
FaultHandlerWithNoError(BR);
FaultHandlerWithNoError(UD);
FaultHandlerWithNoError(NM);
FaultHandlerWithError(DF);
FaultHandlerWithError(TS);
FaultHandlerWithError(NP);
FaultHandlerWithError(SS);
FaultHandlerWithError(GP);
FaultHandlerWithError(PF);
FaultHandlerWithNoError(MF);
FaultHandlerWithError(AC);
FaultHandlerWithNoError(MC);
FaultHandlerWithNoError(XM);
FaultHandlerWithNoError(VE);

__attribute__((interrupt)) void
IntHandlerKeyboard(InterruptFrame *frame) {
  KeyboardOnInterrupt();
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
  DisablePIC();
  PICClearMask(InterruptVector::kKeyboard);

  auto set_idt_entry = [](int irq, auto handler) {
    SetIDTEntry(idt[irq], MakeIDTAttr(DescriptorType::kInterruptGate, 0, true, kISTForTimer), reinterpret_cast<uint64_t>(handler), kKernelCS);
  };

  set_idt_entry(InterruptVector::kKeyboard, IntHandlerKeyboard);
  set_idt_entry(InterruptVector::kLAPICTimer, IntHandlerLAPICTimer);

  set_idt_entry(0, IntHandlerDE);
  set_idt_entry(1, IntHandlerDB);
  set_idt_entry(3, IntHandlerDP);
  set_idt_entry(4, IntHandlerOF);
  set_idt_entry(5, IntHandlerBR);
  set_idt_entry(6, IntHandlerUD);
  set_idt_entry(7, IntHandlerNM);
  set_idt_entry(8, IntHandlerDF);
  set_idt_entry(10, IntHandlerTS);
  set_idt_entry(11, IntHandlerNP);
  set_idt_entry(13, IntHandlerSS);
  set_idt_entry(14, IntHandlerGP);
  set_idt_entry(15, IntHandlerPF);
  set_idt_entry(16, IntHandlerMF);
  set_idt_entry(17, IntHandlerAC);
  set_idt_entry(18, IntHandlerMC);
  set_idt_entry(19, IntHandlerXM);
  set_idt_entry(20, IntHandlerVE);

  LoadIDT(sizeof(idt) - 1, reinterpret_cast<uintptr_t>(&idt[0]));
}
