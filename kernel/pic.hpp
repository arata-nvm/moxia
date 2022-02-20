#pragma once
#include <stdint.h>

void NotifyEOI(uint8_t intno);

void DisablePIC();

void PICSetMask(uint8_t intno);

void PICClearMask(uint8_t intno);

void InitializePIC();
