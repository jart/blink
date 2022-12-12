#ifndef BLINK_SSEFLOAT_H_
#define BLINK_SSEFLOAT_H_
#include "blink/machine.h"

void OpUnpcklpsd(P);
void OpUnpckhpsd(P);
void OpPextrwGdqpUdqIb(P);
void OpPinsrwVdqEwIb(P);
void OpShuffle(P);
void OpShufpsd(P);
void OpSqrtpsd(P);
void OpRsqrtps(P);
void OpRcpps(P);
void OpComissVsWs(P);
void OpAddpsd(P);
void OpMulpsd(P);
void OpSubpsd(P);
void OpDivpsd(P);
void OpMinpsd(P);
void OpMaxpsd(P);
void OpCmppsd(P);
void OpAndpsd(P);
void OpAndnpsd(P);
void OpOrpsd(P);
void OpXorpsd(P);
void OpHaddpsd(P);
void OpHsubpsd(P);
void OpAddsubpsd(P);
void OpMovmskpsd(P);

#endif /* BLINK_SSEFLOAT_H_ */
