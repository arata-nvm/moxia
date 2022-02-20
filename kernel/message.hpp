#pragma once
#include <stdint.h>

struct Message {
  enum Type {
    kTimerTimeout,
    kKeyboardPush,
  } type;

  union {
    struct {
      unsigned long timeout;
      int value;
    } timer;
    struct {
      uint16_t keycode;
    } keyboard;
  } arg;
};
