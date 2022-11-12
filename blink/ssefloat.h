#ifndef BLINK_SSEFLOAT_H_
#define BLINK_SSEFLOAT_H_
#include "blink/machine.h"

void OpUnpcklpsd(struct Machine *, u32);
void OpUnpckhpsd(struct Machine *, u32);
void OpPextrwGdqpUdqIb(struct Machine *, u32);
void OpPinsrwVdqEwIb(struct Machine *, u32);
void OpShuffle(struct Machine *, u32);
void OpShufpsd(struct Machine *, u32);
void OpSqrtpsd(struct Machine *, u32);
void OpRsqrtps(struct Machine *, u32);
void OpRcpps(struct Machine *, u32);
void OpComissVsWs(struct Machine *, u32);
void OpAddpsd(struct Machine *, u32);
void OpMulpsd(struct Machine *, u32);
void OpSubpsd(struct Machine *, u32);
void OpDivpsd(struct Machine *, u32);
void OpMinpsd(struct Machine *, u32);
void OpMaxpsd(struct Machine *, u32);
void OpCmppsd(struct Machine *, u32);
void OpAndpsd(struct Machine *, u32);
void OpAndnpsd(struct Machine *, u32);
void OpOrpsd(struct Machine *, u32);
void OpXorpsd(struct Machine *, u32);
void OpHaddpsd(struct Machine *, u32);
void OpHsubpsd(struct Machine *, u32);
void OpAddsubpsd(struct Machine *, u32);

#endif /* BLINK_SSEFLOAT_H_ */
