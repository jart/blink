#ifndef BLINK_JITDUMP_H_
#define BLINK_JITDUMP_H_
#include <stddef.h>

#include "blink/types.h"

#if defined(__linux) && !defined(TINY)
#define SUPPORT_JITDUMP 1
#else
#define SUPPORT_JITDUMP 0
#endif

#define JITDUMP_MAGIC   0x4A695444
#define JITDUMP_VERSION 1

enum JitDumpType {
  JIT_CODE_LOAD,
  JIT_CODE_MOVE,
  JIT_CODE_DEBUG_INFO,
  JIT_CODE_CLOSE,
  JIT_CODE_MAX
};

struct JitDumpHeader {
  u32 magic;
  u32 version;
  u32 total_size;
  u32 elf_mach;
  u32 pad1;
  u32 pid;
  u64 timestamp;
  u64 flags;
};

struct JitDumpPrefix {
  enum JitDumpType id;
  u32 total_size;
  u64 timestamp;
};

struct JitDumpLoad {
  struct JitDumpPrefix p;
  u32 pid;
  u32 tid;
  u64 vma;
  u64 code_addr;
  u64 code_size;
  u64 code_index;
};

struct JitDumpClose {
  struct JitDumpPrefix p;
};

struct JitDumpMove {
  struct JitDumpPrefix p;
  u32 pid;
  u32 tid;
  u64 vma;
  u64 old_code_addr;
  u64 new_code_addr;
  u64 code_size;
  u64 code_index;
};

struct JitDumpDebugEntry {
  u64 addr;
  u32 lineno;
  u32 discrim;
  const char name[0];
};

struct JitDumpDebug {
  struct JitDumpPrefix p;
  u64 code_addr;
  u64 nr_entry;
  struct JitDumpDebugEntry entries[0];
};

int OpenJitDump(void);
int LoadJitDump(int, const u8 *, size_t);
int CloseJitDump(int);

#endif /* BLINK_JITDUMP_H_ */
