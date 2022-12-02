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

char **LoadStrList(struct Machine *, i64);
char *LoadStr(struct Machine *, i64);
int RegisterMemory(struct Machine *, i64, void *, size_t);
u8 *AccessRam(struct Machine *, i64, size_t, void *[2], u8 *, bool);
u8 *BeginLoadStore(struct Machine *, i64, size_t, void *[2], u8 *);
u8 *BeginStore(struct Machine *, i64, size_t, void *[2], u8 *);
u8 *BeginStoreNp(struct Machine *, i64, size_t, void *[2], u8 *);
u8 *CopyFromUser(struct Machine *, void *, i64, u64);
u8 *FindReal(struct Machine *, i64);
u8 *Load(struct Machine *, i64, size_t, u8 *);
u8 *MallocPage(void);
u8 *RealAddress(struct Machine *, i64);
u8 *ReserveAddress(struct Machine *, i64, size_t, bool);
u8 *ResolveAddress(struct Machine *, i64);
void CommitStash(struct Machine *);
void CopyFromUserRead(struct Machine *, void *, i64, u64);
void CopyToUser(struct Machine *, i64, void *, u64);
void CopyToUserWrite(struct Machine *, i64, void *, u64);
void EndStore(struct Machine *, i64, size_t, void *[2], u8 *);
void EndStoreNp(struct Machine *, i64, size_t, void *[2], u8 *);
void ResetRam(struct Machine *);
void SetReadAddr(struct Machine *, i64, u32);
void SetWriteAddr(struct Machine *, i64, u32);
void VirtualSet(struct Machine *, i64, char, u64);

#endif /* BLINK_MEMORY_H_ */
