#include <stdint.h>

struct Regs
{
    union { uint32_t eax; uint16_t ax; struct { uint8_t al, ah; }; };
    union { uint32_t ebx; uint16_t bx; struct { uint8_t bl, bh; }; };
    union { uint32_t ecx; uint16_t cx; struct { uint8_t cl, ch; }; };
    union { uint32_t edx; uint16_t dx; struct { uint8_t dl, dh; }; };
    union { uint32_t esi; uint16_t si; };
    union { uint32_t edi; uint16_t di; };
    union { uint32_t cflag; };
};

uint16_t int386(uint8_t inter_no, struct Regs* in_regs, struct Regs* out_regs);
