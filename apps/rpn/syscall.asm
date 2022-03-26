bits 64
section .text

global SyscallWrite
SyscallWrite:
  mov rax, 0x80000000
  mov r10, rcx
  syscall
  ret
