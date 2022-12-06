#ifndef BLINK_MOP_H_
#define BLINK_MOP_H_
#include <limits.h>
#include <stdatomic.h>

#include "blink/builtin.h"
#include "blink/endian.h"

/**
 * @fileoverview Memory Loads and Stores.
 *
 * These functions are intended for memory operations where the guest
 * program is interacting with random access memory. Unlike the register
 * file which is thread-local, RAM operations need to have acquire and
 * release semantics since x86 is a strongly ordered architecture. x86
 * also guarantees word-sized memory operations happen atomically when
 * they're word-size aligned.
 *
 * Acquire operations appear to complete before other memory operations
 * subsequently performed by the same thread, as seen by other threads.
 * Release operations appear to complete after other memory operations
 * previously performed by the same thread, as seen by other threads.
 */

MICRO_OP_SAFE u8 Load8(const u8 p[1]) {
  u8 w = Read8(p);
  atomic_thread_fence(memory_order_acquire);
  return w;
}

MICRO_OP_SAFE u16 Load16(const u8 p[2]) {
  u16 w = Read16(p);
  atomic_thread_fence(memory_order_acquire);
  return w;
}

MICRO_OP_SAFE u32 Load32(const u8 p[4]) {
  u32 w = Read32(p);
  atomic_thread_fence(memory_order_acquire);
  return w;
}

MICRO_OP_SAFE u64 Load64(const u8 p[8]) {
  u64 w = Read64(p);
  atomic_thread_fence(memory_order_acquire);
  return w;
}

MICRO_OP_SAFE void Store8(u8 p[1], u8 x) {
  atomic_thread_fence(memory_order_release);
  Write8(p, x);
}

MICRO_OP_SAFE void Store16(u8 p[2], u16 x) {
  atomic_thread_fence(memory_order_release);
  Write16(p, x);
}

MICRO_OP_SAFE void Store32(u8 p[4], u32 x) {
  atomic_thread_fence(memory_order_release);
  Write32(p, x);
}

MICRO_OP_SAFE void Store64(u8 p[8], u64 x) {
  atomic_thread_fence(memory_order_release);
  Write64(p, x);
}

i64 ReadRegisterSigned(u64, u8[8]);
u64 ReadMemory(u64, u8[8]);
u64 ReadMemorySigned(u64, u8[8]);
u64 ReadRegister(u64, u8[8]);
void WriteMemory(u64, u8[8], u64);
void WriteRegister(u64, u8[8], u64);
void WriteRegisterOrMemory(u64, u8[8], u64);

#endif /* BLINK_MOP_H_ */
