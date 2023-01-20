#ifndef BLINK_SSE_H_
#define BLINK_SSE_H_
#include "blink/builtin.h"
#include "blink/machine.h"
#include "blink/modrm.h"
#include "blink/tsan.h"

void OpSsePabsb(P);
void OpSsePabsd(P);
void OpSsePabsw(P);
void OpSsePackssdw(P);
void OpSsePacksswb(P);
void OpSsePackuswb(P);
void OpSsePaddb(P);
void OpSsePaddd(P);
void OpSsePaddq(P);
void OpSsePaddsb(P);
void OpSsePaddsw(P);
void OpSsePaddusb(P);
void OpSsePaddusw(P);
void OpSsePaddw(P);
void OpSsePalignr(P);
void OpSsePand(P);
void OpSsePandn(P);
void OpSsePavgb(P);
void OpSsePavgw(P);
void OpSsePcmpeqb(P);
void OpSsePcmpeqd(P);
void OpSsePcmpeqw(P);
void OpSsePcmpgtb(P);
void OpSsePcmpgtd(P);
void OpSsePcmpgtw(P);
void OpSsePhaddd(P);
void OpSsePhaddsw(P);
void OpSsePhaddw(P);
void OpSsePhsubd(P);
void OpSsePhsubsw(P);
void OpSsePhsubw(P);
void OpSsePmaddubsw(P);
void OpSsePmaddwd(P);
void OpSsePmaxsw(P);
void OpSsePmaxub(P);
void OpSsePminsw(P);
void OpSsePminub(P);
void OpSsePmulhrsw(P);
void OpSsePmulhuw(P);
void OpSsePmulhw(P);
void OpSsePmulld(P);
void OpSsePmullw(P);
void OpSsePmuludq(P);
void OpSsePor(P);
void OpSsePsadbw(P);
void OpSsePshufb(P);
void OpSsePsignb(P);
void OpSsePsignd(P);
void OpSsePsignw(P);
void OpSsePslldv(P);
void OpSsePsllqv(P);
void OpSsePsllwv(P);
void OpSsePsradv(P);
void OpSsePsrawv(P);
void OpSsePsrldv(P);
void OpSsePsrlqv(P);
void OpSsePsrlwv(P);
void OpSsePsubb(P);
void OpSsePsubd(P);
void OpSsePsubq(P);
void OpSsePsubsb(P);
void OpSsePsubsw(P);
void OpSsePsubusb(P);
void OpSsePsubusw(P);
void OpSsePsubw(P);
void OpSsePunpckhbw(P);
void OpSsePunpckhdq(P);
void OpSsePunpckhqdq(P);
void OpSsePunpckhwd(P);
void OpSsePunpcklbw(P);
void OpSsePunpckldq(P);
void OpSsePunpcklqdq(P);
void OpSsePunpcklwd(P);
void OpSsePxor(P);

void MmxPcmpgtb(u8[8], const u8[8]);
void MmxPcmpgtw(u8[8], const u8[8]);
void MmxPcmpeqb(u8[8], const u8[8]);
void MmxPcmpeqw(u8[8], const u8[8]);
void MmxPmullw(u8[8], const u8[8]);
void MmxPminub(u8[8], const u8[8]);
void MmxPaddusw(u8[8], const u8[8]);
void MmxPmaxub(u8[8], const u8[8]);
void MmxPavgb(u8[8], const u8[8]);
void MmxPavgw(u8[8], const u8[8]);
void MmxPmulhw(u8[8], const u8[8]);
void MmxPsubsw(u8[8], const u8[8]);
void MmxPaddsw(u8[8], const u8[8]);
void MmxPsubb(u8[8], const u8[8]);
void MmxPsubw(u8[8], const u8[8]);
void MmxPaddb(u8[8], const u8[8]);
void MmxPaddw(u8[8], const u8[8]);
void MmxPhaddsw(u8[8], const u8[8]);
void MmxPhsubsw(u8[8], const u8[8]);
void MmxPabsb(u8[8], const u8[8]);

static dontinline void OpSse(P, void MmxKernel(u8[8], const u8[8]),
                             void SseKernel(u8[16], const u8[16])) {
  IGNORE_RACES_START();
  if (Osz(rde)) {
    SseKernel(XmmRexrReg(m, rde), GetXmmAddress(A));
  } else {
    MmxKernel(XmmRexrReg(m, rde), GetMmxAddress(A));
  }
  IGNORE_RACES_END();
  if (IsMakingPath(m)) {
    Jitter(A,
           "z4P"    // res0 = GetXmmOrMemPointer(RexbRm)
           "r0s1="  // sav1 = res0
           "z4Q"    // res0 = GetXmmPointer(RexrReg)
           "s1a1="  // arg1 = sav1
           "t"      // arg0 = res0
           "c",     // call function
           Osz(rde) ? SseKernel : MmxKernel);
  }
}

#endif /* BLINK_SSE_H_ */
