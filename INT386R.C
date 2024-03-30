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
  /* the next instruction, so it     */   \
  /* can be patched to an interrupt. */   \
  "call prepare"                          \
  "nop" /* two bytes to hold the */       \
  "nop" /* interrupt instruction */       \
  "jmp save_result"                       \
                                          \
  "prepare:"/*--------------------------*/\
  /* patch int instruction */             \
  "mov al, 0xcd" /* interrupt opcode */   \
  "mov ah, byte ptr [bp+2]"/* inter_no */ \
  "pop bx" /* ret addr*/                  \
  "push bx"                               \
  "mov word ptr cs:[bx], ax"              \
                                          \
  /* ds:bx = &in_regs */                  \
  "mov bx, word ptr [bp+4]"               \
  "mov ds, word ptr [bp+6]"               \
                                          \
  /* load regs from in_regs */            \
  "mov eax, dword ptr ds:[bx]"            \
  "push dword ptr ds:[bx+4]" /*ebx*/      \
  "mov ecx, dword ptr ds:[bx+8]"          \
  "mov edx, dword ptr ds:[bx+12]"         \
  "mov esi, dword ptr ds:[bx+16]"         \
  "mov edi, dword ptr ds:[bx+20]"         \
  "push dword ptr ds:[bx+24]" /*cflag*/   \
  "popfd"                                 \
  "pop ebx"                               \
                                          \
  /* return to the patched instruction */ \
  "ret"                                   \
                                          \
  "save_result:"/*----------------------*/\
  /* ds:bx = &out_regs */                 \
  "push ebx"                              \
  "mov bx, word ptr [bp+8]"               \
  "mov ds, word ptr [bp+10]"              \
                                          \
  /* save regs to out_regs */             \
  "mov dword ptr ds:[bx], eax"            \
  "pop dword ptr ds:[bx+4]" /*ebx*/       \
  "mov dword ptr ds:[bx+8], ecx"          \
  "mov dword ptr ds:[bx+12], edx"         \
  "mov dword ptr ds:[bx+16], esi"         \
  "mov dword ptr ds:[bx+20], edi"         \
  "pushfd"                                \
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
