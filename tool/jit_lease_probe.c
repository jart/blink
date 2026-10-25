/* Native opt-in probe: exhaust the JIT pool while one builder holds a block. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "blink/flag.h"
#include "blink/jit.h"

void AssertFailed(const char *file, int line, const char *message) {
  fprintf(stderr, "%s:%d: %s\n", file, line, message);
  abort();
}

struct Machine;
/* No guest executes in this native probe. Satisfy the library callback. */
void TerminateSignal(struct Machine *m, int sig, int code) {
  (void)m;
  (void)sig;
  (void)code;
  abort();
}

static void Staging(void) {}
static unsigned char padding[kJitBlockSize];

int main(void) {
  struct Jit jit;
  struct JitBlock *held, *other;
  int i;
  FLAG_pagesize = sysconf(_SC_PAGESIZE);
  if (InitJit(&jit, (uintptr_t)Staging)) return 1;
  if (!(held = StartJit(&jit, 0x1000))) return 2;
  if (!AppendJit(held, "test", 4) || !RecordJitJump(held, 0x2000, 0)) return 3;
  for (i = 0; i < 2 * kJitMemorySize / kJitBlockSize; ++i) {
    if (!(other = StartJit(&jit, 0x3000 + i * 4096))) return 4;
    if (!AppendJit(other, padding, sizeof(padding))) return 5;
    if (!FinishJit(&jit, other)) return 6;
    if (held->index != 4 || memcmp(held->addr, "test", 4) || !held->jumps)
      return 7;
  }
  AbandonJit(&jit, held);
  DestroyJit(&jit);
  ShutdownJit();
  return 0;
}
