// Replacement for Watcom's int386 for use in real mode programs.
// Compile using wcl -3
#include "int386r.h"


static uint16_t int386r(uint8_t inter_no, struct Regs far* in_regs, struct Regs far* out_regs);
#pragma aux int386r =                     \
  "enter 0, 0"                            \
  "pushf"                                 \
  "push ds"                               \
                                          \
  /* Using call to get a pointer to  */   \
  /* the next instruction, so the    */   \
  /* interrupt number can be patched.*/   \
  "call prepare"                          \
  "db 0xcd" /* interrupt opcode */        \
  "db 0x00" /* interrupt number */        \
  "jmp save_result"                       \
                                          \
  "prepare:"/*--------------------------*/\
  /* patch int instruction */             \
  "mov ah, byte ptr [bp+2]"/* inter_no */ \
  "mov bx, word ptr [bp-6]"/* ret addr */ \
  "mov byte ptr cs:[bx+1], ah"            \
                                          \
  /* ds:bx = &in_regs */                  \
  "lds bx, [bp+4]"                        \
                                          \
  /* load regs from in_regs */            \
  "mov eax, dword ptr ds:[bx]"            \
  "mov ecx, dword ptr ds:[bx+8]"          \
  "mov edx, dword ptr ds:[bx+12]"         \
  "mov esi, dword ptr ds:[bx+16]"         \
  "mov edi, dword ptr ds:[bx+20]"         \
  "push dword ptr ds:[bx+24]" /*cflag*/   \
  "popfd"                                 \
  "mov ebx, dword ptr ds:[bx+4]"          \
                                          \
  /* return to the patched instruction */ \
  "ret"                                   \
                                          \
  "save_result:"/*----------------------*/\
  /* use the stack for flags and ebx */   \
  "pushfd"                                \
  "push ebx"                              \
                                          \
  /* ds:bx = &out_regs */                 \
  "lds bx, [bp+8]"                        \
                                          \
  /* save regs to out_regs */             \
  "mov dword ptr ds:[bx], eax"            \
  "pop dword ptr ds:[bx+4]" /*ebx*/       \
  "mov dword ptr ds:[bx+8], ecx"          \
  "mov dword ptr ds:[bx+12], edx"         \
  "mov dword ptr ds:[bx+16], esi"         \
  "mov dword ptr ds:[bx+20], edi"         \
  "pop dword ptr ds:[bx+24]" /*cflag*/    \
                                          \
  "pop ds"                                \
  "popf"                                  \
  "leave"                                 \
  parm []                                 \
  modify [bx cx dx si di]                 \
  value [ax]


uint16_t int386(uint8_t inter_no, struct Regs* in_regs, struct Regs* out_regs)
{
  return int386r(inter_no, in_regs, out_regs);
}
