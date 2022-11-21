#ifndef BLINK_SSEFLOAT_H_
#define BLINK_SSEFLOAT_H_
#include "blink/machine.h"

void OpUnpcklpsd(struct Machine *, DISPATCH_PARAMETERS);
void OpUnpckhpsd(struct Machine *, DISPATCH_PARAMETERS);
void OpPextrwGdqpUdqIb(struct Machine *, DISPATCH_PARAMETERS);
void OpPinsrwVdqEwIb(struct Machine *, DISPATCH_PARAMETERS);
void OpShuffle(struct Machine *, DISPATCH_PARAMETERS);
void OpShufpsd(struct Machine *, DISPATCH_PARAMETERS);
void OpSqrtpsd(struct Machine *, DISPATCH_PARAMETERS);
void OpRsqrtps(struct Machine *, DISPATCH_PARAMETERS);
void OpRcpps(struct Machine *, DISPATCH_PARAMETERS);
void OpComissVsWs(struct Machine *, DISPATCH_PARAMETERS);
void OpAddpsd(struct Machine *, DISPATCH_PARAMETERS);
void OpMulpsd(struct Machine *, DISPATCH_PARAMETERS);
void OpSubpsd(struct Machine *, DISPATCH_PARAMETERS);
void OpDivpsd(struct Machine *, DISPATCH_PARAMETERS);
void OpMinpsd(struct Machine *, DISPATCH_PARAMETERS);
void OpMaxpsd(struct Machine *, DISPATCH_PARAMETERS);
void OpCmppsd(struct Machine *, DISPATCH_PARAMETERS);
void OpAndpsd(struct Machine *, DISPATCH_PARAMETERS);
void OpAndnpsd(struct Machine *, DISPATCH_PARAMETERS);
void OpOrpsd(struct Machine *, DISPATCH_PARAMETERS);
void OpXorpsd(struct Machine *, DISPATCH_PARAMETERS);
void OpHaddpsd(struct Machine *, DISPATCH_PARAMETERS);
void OpHsubpsd(struct Machine *, DISPATCH_PARAMETERS);
void OpAddsubpsd(struct Machine *, DISPATCH_PARAMETERS);

#endif /* BLINK_SSEFLOAT_H_ */
