#include "keyboard.hpp"
#include "asmfunc.hpp"
#include "interrupt.hpp"
#include "pic.hpp"
#include "printk.hpp"

const int kPortKeyboardData = 0x60;

void KeyboardOnInterrupt() {
  uint8_t scancode = Inb(kPortKeyboardData);
  printk("%02x\n", scancode);
  NotifyEOI(InterruptVector::kKeyboard);
}
