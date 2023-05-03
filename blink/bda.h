#ifndef BLINK_BDA_H_
#define BLINK_BDA_H_
#include "blink/blinkenlights.h"
#include "blink/machine.h"
#include "blink/endian.h"

#define GetBda8(o)     Read8(m->system->real + 0x400 + (o))
#define SetBda8(o, v)  Write8(m->system->real + 0x400 + (o), (v))
#define GetBda16(o)    Read16(m->system->real + 0x400 + (o))
#define SetBda16(o, v) Write16(m->system->real + 0x400 + (o), (v))
#define GetBda32(o)    Read32(m->system->real + 0x400 + (o))
#define SetBda32(o, v) Write32(m->system->real + 0x400 + (o), (v))

#define    BdaCom1          GetBda16(0x00)
#define SetBdaCom1(v)       SetBda16(0x00, (v))
#define    BdaEbda          GetBda16(0x0E)
#define SetBdaEbda(v)       SetBda16(0x0E, (v))

#define    BdaEquip         GetBda16(0x10)
#define SetBdaEquip(v)      SetBda16(0x10, (v))
#define    BdaMemsz         GetBda16(0x13)
#define SetBdaMemsz(v)      SetBda16(0x13, (v))

#define    BdaVmode         GetBda8(0x49)
#define SetBdaVmode(v)      SetBda8(0x49, (v))
#define    BdaCols          GetBda16(0x4A)       // returns actual # columns
#define SetBdaCols(v)       SetBda16(0x4A, (v))  // BIOS stores # columns
#define    BdaPagesz        GetBda16(0x4C)
#define SetBdaPagesz(v)     SetBda16(0x4C, (v))
#define    BdaCurx          GetBda8(0x50)
#define SetBdaCurx(v)       SetBda8(0x50, (v))
#define    BdaCury          GetBda8(0x51)
#define SetBdaCury(v)       SetBda8(0x51, (v))
#define    BdaCurend        GetBda8(0x60)
#define SetBdaCurend(v)     SetBda8(0x60, (v))
#define    BdaCurstart      GetBda8(0x61)
#define SetBdaCurstart(v)   SetBda8(0x61, (v))
#define BdaCurhidden        ((BdaCurstart & 0x20) || (BdaCurstart > BdaCurend))
#define    BdaCurpage       GetBda8(0x62)
#define SetBdaCurpage(v)    SetBda8(0x62, (v))
#define    BdaCrtc          GetBda16(0x64)
#define SetBdaCrtc(v)       SetBda16(0x64, (v))

#define    BdaTimer         GetBda32(0x6C)
#define SetBdaTimer(v)      SetBda32(0x6C, (v))
#define    Bda24hr          GetBda8(0x70)
#define SetBda24hr(v)       SetBda8(0x70, (v))

#define    BdaLines         (GetBda8(0x84) + 1)     // returns actual # lines
#define SetBdaLines(v)      SetBda8(0x84, (v) - 1) // BIOS stores # lines - 1

#endif /* BLINK_BDA_H_ */
