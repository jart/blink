#!/bin/sh
make -j16 m=rel || exit

if [ ! -d "$HOME/cosmo" ]; then
  echo 'need ~/cosmo directory' >&2
  exit 1
fi
(
  cd ~/cosmo
  make -j16 o//test
)

tests="
$HOME/cosmo/o/test/libc/intrin/kprintf_test.com
$HOME/cosmo/o/test/libc/intrin/intrin_test.com
$HOME/cosmo/o/test/libc/intrin/countbits_test.com
$HOME/cosmo/o/test/libc/intrin/asan_test.com
$HOME/cosmo/o/test/libc/intrin/bextra_test.com
$HOME/cosmo/o/test/libc/intrin/bitreverse_test.com
$HOME/cosmo/o/test/libc/intrin/describeflags_test.com
$HOME/cosmo/o/test/libc/intrin/describegidlist_test.com
$HOME/cosmo/o/test/libc/intrin/describesigset_test.com
$HOME/cosmo/o/test/libc/intrin/division_test.com
$HOME/cosmo/o/test/libc/intrin/dos2errno_test.com
$HOME/cosmo/o/test/libc/intrin/formatint32_test.com
$HOME/cosmo/o/test/libc/intrin/getenv_test.com
$HOME/cosmo/o/test/libc/intrin/integralarithmetic_test.com
$HOME/cosmo/o/test/libc/intrin/morton_test.com
$HOME/cosmo/o/test/libc/intrin/palignr_test.com
$HOME/cosmo/o/test/libc/intrin/pmulhrsw_test.com
$HOME/cosmo/o/test/libc/intrin/popcnt_test.com
$HOME/cosmo/o/test/libc/intrin/pshuf_test.com
$HOME/cosmo/o/test/libc/intrin/putenv_test.com
$HOME/cosmo/o/test/libc/intrin/rand64_test.com
$HOME/cosmo/o/test/libc/intrin/rounddown2pow_test.com
$HOME/cosmo/o/test/libc/intrin/roundup2log_test.com
$HOME/cosmo/o/test/libc/intrin/roundup2pow_test.com
$HOME/cosmo/o/test/libc/intrin/sched_yield_test.com
$HOME/cosmo/o/test/libc/intrin/strlen_test.com
$HOME/cosmo/o/test/libc/intrin/strsignal_r_test.com
"

for f in $tests; do
  e=o/rel/blink/blink
  if (time $f) >/tmp/blink-bench.sh.tmp 2>&1; then
    if (time $e $f) >>/tmp/blink-bench.sh.tmp 2>&1; then
      echo $(grep real </tmp/blink-bench.sh.tmp) $f
    else
      echo FAILED $e $f
    fi
  else
    echo FAILED $f
  fi
done
