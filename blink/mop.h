#ifndef BLINK_MOP_H_
#define BLINK_MOP_H_
#include <limits.h>
#include <stdatomic.h>

#include "blink/builtin.h"
#include "blink/endian.h"
#include "blink/spin.h"
#include "blink/tsan.h"

#if !defined(__x86_64__) && !defined(__i386__)
#define FENCE atomic_thread_fence(memory_order_seq_cst)
#else
#define FENCE (void)0
#endif

void InitBus(void);
void LockBus(const u8 *);
void UnlockBus(const u8 *);

i64 Load8(const u8[1]);
i64 Load16(const u8[2]);
i64 Load32(const u8[4]);
i64 Load64(const u8[8]);
i64 Load64Unlocked(const u8[8]);

void Store8(u8[1], u64);
void Store16(u8[2], u64);
void Store32(u8[4], u64);
void Store64(u8[8], u64);
void Store64Unlocked(u8[8], u64);

i64 ReadRegisterSigned(u64, u8[8]);
u64 ReadMemory(u64, u8[8]);
u64 ReadMemorySigned(u64, u8[8]);
u64 ReadMemoryUnlocked(u64, u8[8]);
u64 ReadRegister(u64, u8[8]);
void WriteMemory(u64, u8[8], u64);
void WriteRegister(u64, u8[8], u64);
void WriteRegisterOrMemory(u64, u8[8], u64);

#endif /* BLINK_MOP_H_ */
