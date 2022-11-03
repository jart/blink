#ifndef BLINK_SSEFLOAT_H_
#define BLINK_SSEFLOAT_H_
#include "blink/machine.h"

void OpUnpcklpsd(struct Machine *, uint32_t);
void OpUnpckhpsd(struct Machine *, uint32_t);
void OpPextrwGdqpUdqIb(struct Machine *, uint32_t);
void OpPinsrwVdqEwIb(struct Machine *, uint32_t);
void OpShuffle(struct Machine *, uint32_t);
void OpShufpsd(struct Machine *, uint32_t);
void OpSqrtpsd(struct Machine *, uint32_t);
void OpRsqrtps(struct Machine *, uint32_t);
void OpRcpps(struct Machine *, uint32_t);
void OpComissVsWs(struct Machine *, uint32_t);
void OpAddpsd(struct Machine *, uint32_t);
void OpMulpsd(struct Machine *, uint32_t);
void OpSubpsd(struct Machine *, uint32_t);
void OpDivpsd(struct Machine *, uint32_t);
void OpMinpsd(struct Machine *, uint32_t);
void OpMaxpsd(struct Machine *, uint32_t);
void OpCmppsd(struct Machine *, uint32_t);
void OpAndpsd(struct Machine *, uint32_t);
void OpAndnpsd(struct Machine *, uint32_t);
void OpOrpsd(struct Machine *, uint32_t);
void OpXorpsd(struct Machine *, uint32_t);
void OpHaddpsd(struct Machine *, uint32_t);
void OpHsubpsd(struct Machine *, uint32_t);
void OpAddsubpsd(struct Machine *, uint32_t);

#endif /* BLINK_SSEFLOAT_H_ */
