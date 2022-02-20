#pragma once

const uint16_t kKeyCharMask = 0x00ff;
const uint16_t kKeyShiftMask = 0x0100;
const uint16_t kKeyCtrlMask = 0x0200;
const uint16_t kKeyAltMask = 0x0400;

void KeyboardOnInterrupt();

void InitializeKeyboard();
