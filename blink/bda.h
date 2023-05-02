#ifndef BLINK_BDA_H_
#define BLINK_BDA_H_
#include "blink/machine.h"
#include "blink/endian.h"

#define BDA(address)        (g_machine->system->real + address)

#define    BdaVmode         Read8(BDA(0x0449))
#define SetBdaVmode(v)      Write8(BDA(0x0449),v)
#define    BdaCols          Read16(BDA(0x044A))         // returns actual # columns
#define SetBdaCols(v)       Write16(BDA(0x044A),v)      // BIOS stores # columns
#define    BdaLines         (Read8(BDA(0x0484)) + 1)    // returns actual # lines
#define SetBdaLines(v)      Write8(BDA(0x0484),v)       // BIOS stores # lines - 1
#define    BdaPagesz        Read16(BDA(0x044C))
#define SetBdaPagesz(v)     Write16(BDA(0x044C),v)
#define    BdaCurx          Read8(BDA(0x0450))
#define SetBdaCurx(v)       Write8(BDA(0x0450),v)
#define    BdaCury          Read8(BDA(0x0451))
#define SetBdaCury(v)       Write8(BDA(0x0451),v)
#define    BdaCrtc          Read16(BDA(0x0464))
#define SetBdaCrtc(v)       Write16(BDA(0x0464),v)
#define    BdaCurend        Read8(BDA(0x0460))
#define SetBdaCurend(v)     Write8(BDA(0x0460),v)
#define    BdaCurstart      Read8(BDA(0x0461))
#define SetBdaCurstart(v)   Write8(BDA(0x0461),v)
#define BdaCurhidden        ((BdaCurstart & 0x20) || (BdaCurstart > BdaCurend))
#define    BdaCurpage       Read8(BDA(0x0462))
#define SetBdaCurpage(v)    Write8(BDA(0x0462),v)

#define    BdaEquip         Read16(BDA(0x0410))
#define SetBdaEquip(v)      Write16(BDA(0x0410),v)
#define    BdaMemsz         Read16(BDA(0x0413))
#define SetBdaMemsz(v)      Write16(BDA(0x0413),v)
#define    BdaTimer         Read32(BDA(0x046C))
#define SetBdaTimer(v)      Write32(BDA(0x046C),v)
#define    Bda24hr          Read8(BDA(0x0470))
#define SetBda24hr(v)       Write8(BDA(0x0470),v)

#endif /* BLINK_BDA_H_ */
