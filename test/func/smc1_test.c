// tests for class 1 self-modifying code
// this is the portable prim/proper kind
#include <string.h>
#include <sys/mman.h>

const unsigned char kAdd[] = {
    0x90,        // nop
    0x90,        // nop
    0x90,        // nop
    0x90,        // nop
    0x89, 0xf8,  // mov    %edi,%eax
    0x90,        // nop
    0x90,        // nop
    0x90,        // nop
    0x90,        // nop
    0x01, 0xf0,  // add    %esi,%eax
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
    0x89, 0xf8,  // mov    %edi,%eax
    0x90,        // nop
    0x90,        // nop
    0x90,        // nop
    0x29, 0xf0,  // sub    %esi,%eax
    0x90,        // nop
    0x90,        // nop
    0x90,        // nop
    0xc3,        // ret
};

typedef int math_f(int, int);

int main(int argc, char *argv[]) {
  void *p;
  math_f *f;
  p = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  if (p == MAP_FAILED) return 1;
  f = (math_f *)p;
  memcpy(p, kAdd, sizeof(kAdd));
  if (mprotect(p, 4096, PROT_READ | PROT_EXEC)) return 2;
  if (f(20, 3) != 23) return 3;
  if (mprotect(p, 4096, PROT_READ | PROT_WRITE)) return 4;
  memcpy(p, kSub, sizeof(kSub));
  if (mprotect(p, 4096, PROT_READ | PROT_EXEC)) return 5;
  if (f(20, 3) != 17) return 6;
  if (munmap(p, 4096)) return 7;
  return 0;
}
