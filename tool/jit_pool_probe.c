/* Host-side regression: separate JIT managers must never lease overlapping
 * code. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "blink/flag.h"
#include "blink/jit.h"
#include "blink/signal.h"

void TerminateSignal(struct Machine* m, int sig, int code) {
  (void)m;
  (void)sig;
  (void)code;
  abort();
}

static int CheckPool(void) {
  struct Jit jit[2];
  struct JitBlock *blocks[512], *jb;
  int n = 0, i, failed = 0;
  InitJit(&jit[0], 0);
  InitJit(&jit[1], 0);
  while (n < 512 && (jb = StartJit(&jit[n % 2], 0))) {
    for (i = 0; i < n; ++i) {
      if (jb->addr == blocks[i]->addr) {
        fprintf(stderr, "duplicate code block at leases %d and %d\n", i, n);
        failed = 1;
        break;
      }
    }
    blocks[n++] = jb;
    if (failed) break;
  }
  if (!n || n == 512) failed = 1;
  for (i = 0; i < n; ++i) AbandonJit(&jit[i % 2], blocks[i]);
  DestroyJit(&jit[0]);
  DestroyJit(&jit[1]);
  printf("%d leases checked; %s\n", n, failed ? "FAIL" : "PASS");
  return failed;
}

int main(void) {
  int failed;
  FLAG_pagesize = sysconf(_SC_PAGESIZE);
  failed = CheckPool();
  if (!failed) failed = CheckPool(); /* Reuse returned blocks after exec. */
  ShutdownJit();
  if (!failed) failed = CheckPool(); /* Reinitialize after shutdown. */
  ShutdownJit();
  return failed;
}
