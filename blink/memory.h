#ifndef BLINK_MEMORY_H_
#define BLINK_MEMORY_H_
#include "blink/machine.h"

#define PAGE_V    0x01
#define PAGE_RW   0x02
#define PAGE_U    0x04
#define PAGE_4KB  0x80
#define PAGE_2MB  0x0180
#define PAGE_1GB  0x0180
#define PAGE_RSRV 0x0200
#define PAGE_IGN1 0x0e00
#define PAGE_TA   0x00007ffffffff000
#define PAGE_PA2  0x00007fffffe00000
#define PAGE_XD   0x8000000000000000

char **LoadStrList(struct Machine *, int64_t);
int RegisterMemory(struct Machine *, int64_t, void *, size_t);
uint64_t FindPage(struct Machine *, int64_t);
void *AccessRam(struct Machine *, int64_t, size_t, void *[2], uint8_t *, bool);
void *BeginLoadStore(struct Machine *, int64_t, size_t, void *[2], uint8_t *);
void *BeginStore(struct Machine *, int64_t, size_t, void *[2], uint8_t *);
void *BeginStoreNp(struct Machine *, int64_t, size_t, void *[2], uint8_t *);
void *FindReal(struct Machine *, int64_t);
void *Load(struct Machine *, int64_t, size_t, uint8_t *);
void *LoadBuf(struct Machine *, int64_t, size_t);
void *LoadStr(struct Machine *, int64_t);
void *MallocPage(void);
void *RealAddress(struct Machine *, int64_t);
void *ReserveAddress(struct Machine *, int64_t, size_t);
void *ResolveAddress(struct Machine *, int64_t);
void *VirtualSend(struct Machine *, void *, int64_t, uint64_t);
void EndStore(struct Machine *, int64_t, size_t, void *[2], uint8_t *);
void EndStoreNp(struct Machine *, int64_t, size_t, void *[2], uint8_t *);
void ResetRam(struct Machine *);
void SetReadAddr(struct Machine *, int64_t, uint32_t);
void SetWriteAddr(struct Machine *, int64_t, uint32_t);
void VirtualRecv(struct Machine *, int64_t, void *, uint64_t);
void VirtualRecvWrite(struct Machine *, int64_t, void *, uint64_t);
void VirtualSendRead(struct Machine *, void *, int64_t, uint64_t);
void VirtualSet(struct Machine *, int64_t, char, uint64_t);

#endif /* BLINK_MEMORY_H_ */
