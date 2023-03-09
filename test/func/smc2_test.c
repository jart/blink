// tests for class 2 self-modifying code
// this is the kind intel's cpus support
// namely option 1 from the intel manual
// using indirect branches to other page
#include <string.h>
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

typedef int math_f(int, int);

// even though we're copying byte by byte, blink shouldn't segfault on
// every byte, and it shouldn't need to clear caches on every byte--at
// least not in the canonical jit'ed + linear memory optimization mode
//
//     make -j8 o//blink/blink o//test/func/smc2_test.elf
//     o//blink/blink -Z o//test/func/smc2_test.elf
//
void SlowMemCpy(void *dest, const void *src, int size) {
  int i;
  volatile char *d = (volatile char *)dest;
  volatile const char *s = (volatile const char *)src;
  for (i = 0; i < size; ++i) {
    d[i] = s[i];
  }
}

int main(int argc, char *argv[]) {
  int i;
  void *p;
  math_f *f;
  p = mmap(0, 65536, PROT_READ | PROT_WRITE | PROT_EXEC,
           MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (p == MAP_FAILED) return 1;
  f = (math_f *)p;
  for (i = 0; i < 10; ++i) {
    SlowMemCpy(p, kAdd, sizeof(kAdd));
    if (f(20, 3) != 23) return 2;
    SlowMemCpy(p, kSub, sizeof(kSub));
    if (f(20, 3) != 17) return 3;
  }
  if (munmap(p, 65536)) return 4;
  return 0;
}
