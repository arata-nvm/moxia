#include "pic.hpp"
#include "asmfunc.hpp"

const uint16_t kPic1Cmd = 0x20;
const uint16_t kPic1Data = 0x21;
const uint16_t kPic2Cmd = 0xA0;
const uint16_t kPic2Data = 0xA1;

void NotifyEOI(uint8_t intno) {
  if (intno >= 0x28) {
    Outb(kPic2Cmd, 0x20);
  }
  Outb(kPic1Cmd, 0x20);
}

void DisablePIC() {
  Outb(kPic1Data, 0xFF);
  Outb(kPic2Data, 0xFF);
}

void PICSetMask(uint8_t intno) {
  uint8_t irqno = intno - 0x20;
  uint16_t port;
  if (irqno < 8) {
    port = kPic1Data;
  } else {
    port = kPic2Data;
    irqno -= 8;
  }

  uint8_t mask = Inb(port) | (1 << irqno);
  Outb(port, mask);
}

void PICClearMask(uint8_t intno) {
  uint8_t irqno = intno - 0x20;
  uint16_t port;
  if (irqno < 8) {
    port = kPic1Data;
  } else {
    port = kPic2Data;
    irqno -= 8;
  }

  uint8_t mask = Inb(port) & ~(1 << irqno);
  Outb(port, mask);
}

void InitializePIC() {
  // start initialization
  Outb(kPic1Cmd, 0x11);
  Outb(kPic2Cmd, 0x11);

  // PIC vector offset
  Outb(kPic1Data, 0x20); // IRQ0-IRQ7  -> 32-39
  Outb(kPic2Data, 0x28); // IRQ8-IRQ15 -> 40-47

  // how PICs are connected
  Outb(kPic1Data, 0x4);
  Outb(kPic2Data, 0x2);

  // x86 mode
  Outb(kPic1Data, 0x1);
  Outb(kPic2Data, 0x1);

  // masks
  Outb(kPic1Data, 0x0);
  Outb(kPic2Data, 0x0);
}
