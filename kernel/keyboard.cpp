#include "keyboard.hpp"
#include "asmfunc.hpp"
#include "interrupt.hpp"
#include "pic.hpp"
#include "printk.hpp"
#include "task.hpp"

static const uint16_t kPortKeyboardData = 0x60;

static const uint8_t kBreakMask = 0x80;
static const uint8_t kKeyLShift = 0x2a;
static const uint8_t kKeyRShift = 0x36;
static const uint8_t kKeyCtrl = 0x1d;
static const uint8_t kKeyAlt = 0x38;

static uint8_t keyShiftPressed;
static bool keyCtrlPressed;
static bool keyAltPressed;

static const uint8_t kKeymapLower[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0};

static const uint8_t kKeymapUpper[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0, '|',
    'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, '*', 0, ' ',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+', 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0};

void KeyboardOnInterrupt() {
  uint8_t scancode = Inb(kPortKeyboardData);

  if ((scancode & kBreakMask) == 0) {
    // pressed

    switch (scancode) {
    case kKeyLShift:
      keyShiftPressed |= 0xb10;
      break;
    case kKeyRShift:
      keyShiftPressed |= 0xb01;
      break;
    case kKeyCtrl:
      keyCtrlPressed = true;
      break;
    case kKeyAlt:
      keyAltPressed = true;
      break;

    default:
      uint16_t keycode = 0;
      if (keyShiftPressed != 0) {
        keycode = kKeymapUpper[scancode] | kKeyShiftMask;
      } else {
        keycode = kKeymapLower[scancode];
      }
      if (keyCtrlPressed) {
        keycode |= kKeyCtrlMask;
      }
      if (keyAltPressed) {
        keycode |= kKeyAltMask;
      }

      Message m{Message::kKeyboardPush};
      m.arg.keyboard.keycode = keycode;
      task_manager->SendMessage(1, m);
    }
  } else {
    // released

    switch (scancode & ~kBreakMask) {
    case kKeyLShift:
      keyShiftPressed &= ~0xb10;
      break;
    case kKeyRShift:
      keyShiftPressed &= ~0xb01;
      break;
    case kKeyCtrl:
      keyCtrlPressed = false;
      break;
    case kKeyAlt:
      keyAltPressed = false;
      break;
    }
  }

  NotifyEOI(InterruptVector::kKeyboard);
}
