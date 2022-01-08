#pragma once

struct Message {
  enum Type {
    kTimerTimeout,
  } type;

  union {
    struct {
      unsigned long timeout;
      int value;
    } timer;
  } arg;
};
