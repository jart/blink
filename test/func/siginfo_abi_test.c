#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "blink/linux.h"

#define Offsetof(t, f)         \
  ({                           \
    t s = {0};                 \
    (char *)&s.f - (char *)&s; \
  })

#define Sizeof(t, f) \
  ({                 \
    t s = {0};       \
    sizeof(s.f);     \
  })

#define M(f, g)                                                         \
  if (Offsetof(siginfo_t, f) != Offsetof(struct siginfo_linux, g)) {    \
    fprintf(stderr, "bad field offset %s want %zu got %zu\n", #f,       \
            Offsetof(siginfo_t, f), Offsetof(struct siginfo_linux, g)); \
    exit(1);                                                            \
  }                                                                     \
  if (Sizeof(siginfo_t, f) != Sizeof(struct siginfo_linux, g)) {        \
    fprintf(stderr, "bad field size %s want %zu got %zu\n", #f,         \
            Sizeof(siginfo_t, f), Sizeof(struct siginfo_linux, g));     \
    exit(1);                                                            \
  }

int main(int argc, char *argv[]) {
  if (sizeof(siginfo_t) != sizeof(struct siginfo_linux)) {
    fprintf(stderr, "bad struct size %zu vs. %zu\n", sizeof(siginfo_t),
            sizeof(struct siginfo_linux));
    exit(1);
  }
  M(si_addr, addr);
  M(si_addr_lsb, addr_lsb);
  M(si_arch, arch);
  M(si_band, band);
  M(si_call_addr, call_addr);
  M(si_code, code);
  M(si_errno, errno_);
  M(si_fd, fd);
  M(si_lower, lower);
  M(si_overrun, overrun);
  M(si_pid, pid);
  M(si_pkey, pkey);
  M(si_signo, signo);
  M(si_status, status);
  M(si_stime, stime);
  M(si_syscall, syscall);
  M(si_timerid, timerid);
  M(si_uid, uid);
  M(si_upper, upper);
  M(si_utime, utime);
  M(si_value, value);
  return 0;
}
