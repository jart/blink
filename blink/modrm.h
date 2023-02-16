#ifndef BLINK_MODRM_H_
#define BLINK_MODRM_H_
#include "blink/builtin.h"
#include "blink/machine.h"
#include "blink/rde.h"
#include "blink/thread.h"
#include "blink/x86.h"

#define RegRexbSrm(m, x)   m->weg[RexbSrm(x)]
#define AddrByteReg(m, k)  (m->beg + kByteReg[k])
#define ByteRexrReg(m, x)  AddrByteReg(m, RexRexr(x))
#define ByteRexbRm(m, x)   AddrByteReg(m, RexRexb(x))
#define ByteRexbSrm(m, x)  AddrByteReg(m, RexRexbSrm(x))
#define RegSrm(m, x)       m->weg[Srm(x)]
#define RegVreg(m, x)      m->weg[Vreg(x)]
#define RegRexbRm(m, x)    m->weg[RexbRm(x)]
#define RegRexrReg(m, x)   m->weg[RexrReg(x)]
#define RegRexbBase(m, x)  m->weg[RexbBase(x)]
#define RegRexxIndex(m, x) m->weg[Rexx(x) << 3 | SibIndex(x)]
#define MmRm(m, x)         m->xmm[(x & 00000001600) >> 7]
#define MmReg(m, x)        m->xmm[(x & 00000000007) >> 0]
#define XmmRexbRm(m, x)    m->xmm[RexbRm(x)]
#define XmmRexrReg(m, x)   m->xmm[RexrReg(x)]

#ifdef HAVE_THREADS
#define Lock(x) ((x & 020000000000) >> 037)
#else
#define Lock(x) 0
#endif

#ifndef DISABLE_METAL
#define Mode(x)   ((x & 001400000000) >> 032)
#define Eamode(x) ((x & 000300000000) >> 030)
#else
#define Mode(x) XED_MODE_LONG
static inline u32 Eamode(u32 x) {
  u32 res = (x & 000300000000) >> 030;
  if (res == XED_MODE_REAL) __builtin_unreachable();
  return res;
}
#endif

struct AddrSeg {
  i64 addr;
  u64 seg;
};

extern const u8 kByteReg[32];

u8 *GetModrmReadBW(P);
u8 *GetModrmWriteBW(P);
i64 ComputeAddress(P);
struct AddrSeg LoadEffectiveAddress(const P);
u8 *ComputeReserveAddressRead(P, size_t);
u8 *ComputeReserveAddressRead1(P);
u8 *ComputeReserveAddressRead4(P);
u8 *ComputeReserveAddressRead8(P);
u8 *ComputeReserveAddressWrite(P, size_t);
u8 *ComputeReserveAddressWrite1(P);
u8 *ComputeReserveAddressWrite4(P);
u8 *ComputeReserveAddressWrite8(P);
u8 *GetModrmRegisterBytePointerRead1(P);
u8 *GetModrmRegisterBytePointerWrite1(P);
u8 *GetModrmRegisterMmPointerRead(P, size_t);
u8 *GetModrmRegisterMmPointerRead8(P);
u8 *GetModrmRegisterMmPointerWrite(P, size_t);
u8 *GetModrmRegisterMmPointerWrite8(P);
u8 *GetModrmRegisterWordPointerRead(P, size_t);
u8 *GetModrmRegisterWordPointerRead2(P);
u8 *GetModrmRegisterWordPointerRead4(P);
u8 *GetModrmRegisterWordPointerRead8(P);
u8 *GetModrmRegisterWordPointerReadOsz(P);
u8 *GetModrmRegisterWordPointerReadOszRexw(P);
u8 *GetModrmRegisterWordPointerWrite(P, size_t);
u8 *GetModrmRegisterWordPointerWrite2(P);
u8 *GetModrmRegisterWordPointerWrite4(P);
u8 *GetModrmRegisterWordPointerWrite8(P);
u8 *GetModrmRegisterWordPointerWriteOsz(P);
u8 *GetModrmRegisterWordPointerWriteOszRexw(P);
u8 *GetModrmRegisterXmmPointerRead16(P);
u8 *GetModrmRegisterXmmPointerRead4(P);
u8 *GetModrmRegisterXmmPointerRead8(P);
u8 *GetModrmRegisterXmmPointerWrite16(P);
u8 *GetModrmRegisterXmmPointerWrite4(P);
u8 *GetModrmRegisterXmmPointerWrite8(P);
u8 *GetXmmAddress(P) returnsaligned((16));
u8 *GetMmxAddress(P) returnsaligned((8));

#endif /* BLINK_MODRM_H_ */
