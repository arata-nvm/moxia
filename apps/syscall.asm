bits 64
section .text

global SyscallRead
SyscallRead:
  mov rax, 0x80000000
  mov r10, rcx
  syscall
  ret

global SyscallWrite
SyscallWrite:
  mov rax, 0x80000001
  mov r10, rcx
  syscall
  ret

global SyscallOpen
SyscallOpen:
  mov rax, 0x80000002
  mov r10, rcx
  syscall
  ret

global SyscallExit
SyscallExit:
  mov rax, 0x80000003
  mov r10, rcx
  syscall
  ret
