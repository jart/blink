// tests for bsd sysctl() system call
#include <sys/types.h>
// ordering sometimes matters
#include <sys/sysctl.h>
#include <sys/time.h>

int features_we_care_about[] = {
    CTL_HW,
    CTL_KERN,
    KERN_BOOTTIME,
#ifdef HW_MEMSIZE
    HW_MEMSIZE,
#else
    HW_PHYSMEM,
#endif
};

int main(int argc, char *argv[]) {
  struct timeval x;
  size_t n = sizeof(x);
  int mib[] = {CTL_KERN, KERN_BOOTTIME};
  if (sysctl(mib, 2, &x, &n, 0, 0) == -1) return 1;
  return 0;
}
