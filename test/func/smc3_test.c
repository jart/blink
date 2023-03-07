// tests for class 3 self-modifying code
// this is the kind intel's cpus support
// namely option 1 from the intel manual
// using a near branch to the local page
#include <string.h>
#include <sys/auxv.h>
#include <sys/mman.h>

// To write self-modifying code and ensure that it is compliant with
// current and future versions of the IA-32 architectures, use one of
// the following coding options:
//
// (* OPTION 1 *)
// Store modified code (as data) into code segment;
// Jump to new code or an intermediate location;
// Execute new code;
//
// (* OPTION 2 *)
// Store modified code (as data) into code segment;
// Execute a serializing instruction; (* For example, CPUID instruction *)
// Execute new code;
//
// ──Intel V.3 §8.1.3

const unsigned char kAdd[] = {
    0x90,        // nop
    0x90,        // nop
    0x90,        // nop
    0x90,        // nop
    0x89, 0xf8,  // mov %edi,%eax
    0x90,        // nop
    0x90,        // nop
    0x90,        // nop
    0x90,        // nop
    0x01, 0xf0,  // add %esi,%eax
    0x90,        // nop
    0x90,        // nop
    0x90,        // nop
    0x90,        // nop
    0xc3,        // ret
};

const unsigned char kSub[] = {
    0x90,        // nop
    0x90,        // nop
    0x90,        // nop
    0x89, 0xf8,  // mov %edi,%eax
    0x90,        // nop
    0x90,        // nop
    0x90,        // nop
    0x90,        // nop
    0x29, 0xf0,  // sub %esi,%eax
    0x90,        // nop
    0x90,        // nop
    0x90,        // nop
    0xc3,        // ret
};

__attribute__((__noinline__)) static int f(int x, int y) {
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  asm("nop");
  return 0;
}

__attribute__((__noinline__)) static int Main(void) {
  int i, prot;
  long pagesz;
  prot = PROT_READ | PROT_WRITE | PROT_EXEC;
  if ((pagesz = getauxval(AT_PAGESZ)) == -1) return 1;
  if (mprotect((void *)((long)f & -pagesz), pagesz, prot)) return 2;
  if (f(20, 3) != 0) return 3;
  for (i = 0; i < 10; ++i) {
    memcpy(f, kAdd, sizeof(kAdd));
    if (f(20, 3) != 23) return i * 10 + 4;
    memcpy(f, kSub, sizeof(kSub));
    if (f(20, 3) != 17) return i * 10 + 5;
  }
  return 0;
}

int main(int argc, char *argv[]) {
  return Main();
}
