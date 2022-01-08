#pragma once

#include <stdint.h>

extern "C" {
void LoadIDT(uint16_t limit, uint64_t offset);
void LoadGDT(uint16_t limit, uint64_t offset);
void SetDSAll(uint16_t value);
void SetCSSS(uint16_t cs, uint16_t ss);
uint64_t GetCR3();
void SetCR3(uint64_t value);
void SwitchContext(void *next_ctx, void *current_ctx);
}
