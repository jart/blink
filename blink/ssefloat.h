#ifndef BLINK_SSEFLOAT_H_
#define BLINK_SSEFLOAT_H_
#include "blink/machine.h"

void OpUnpcklpsd(struct Machine *, u64);
void OpUnpckhpsd(struct Machine *, u64);
void OpPextrwGdqpUdqIb(struct Machine *, u64);
void OpPinsrwVdqEwIb(struct Machine *, u64);
void OpShuffle(struct Machine *, u64);
void OpShufpsd(struct Machine *, u64);
void OpSqrtpsd(struct Machine *, u64);
void OpRsqrtps(struct Machine *, u64);
void OpRcpps(struct Machine *, u64);
void OpComissVsWs(struct Machine *, u64);
void OpAddpsd(struct Machine *, u64);
void OpMulpsd(struct Machine *, u64);
void OpSubpsd(struct Machine *, u64);
void OpDivpsd(struct Machine *, u64);
void OpMinpsd(struct Machine *, u64);
void OpMaxpsd(struct Machine *, u64);
void OpCmppsd(struct Machine *, u64);
void OpAndpsd(struct Machine *, u64);
void OpAndnpsd(struct Machine *, u64);
void OpOrpsd(struct Machine *, u64);
void OpXorpsd(struct Machine *, u64);
void OpHaddpsd(struct Machine *, u64);
void OpHsubpsd(struct Machine *, u64);
void OpAddsubpsd(struct Machine *, u64);

#endif /* BLINK_SSEFLOAT_H_ */
