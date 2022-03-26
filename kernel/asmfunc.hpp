#pragma once

#include <stdint.h>

extern "C" {
uint8_t Inb(uint16_t port);
void Outb(uint16_t port, uint8_t value);
void LoadIDT(uint16_t limit, uint64_t offset);
void LoadGDT(uint16_t limit, uint64_t offset);
void LoadTR(uint16_t sel);
void SetDSAll(uint16_t value);
void SetCSSS(uint16_t cs, uint16_t ss);
uint64_t GetCR3();
void SetCR3(uint64_t value);
void SwitchContext(void *next_ctx, void *current_ctx);
void RestoreContext(void *task_context);
int CallApp(int argc, char **argv, uint16_t ss, uint64_t rip, uint64_t rsp, uint64_t *os_stack_ptr);
void IntHandlerLAPICTimer();
void WriteMSR(uint32_t msr, uint64_t value);
void SyscallEntry();
}
