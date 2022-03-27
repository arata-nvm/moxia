#pragma once
#include <stdint.h>

struct Message {
  enum Type {
    kTimerTimeout,
    kKeyboardPush,
    kPipe,
  } type;

  union {
    struct {
      unsigned long timeout;
      int value;
    } timer;
    struct {
      uint16_t keycode;
    } keyboard;
    struct {
      char data[16];
      uint8_t len;
    } pipe;
  } arg;
};
