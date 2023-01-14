#!/bin/sh
m=${1:-tiny}

F="
o/$m/blink/blink
o/$m/x86_64/blink/blink
o/$m/aarch64/blink/blink
o/$m/i486/blink/blink
"

make -j8 $F m=$m || exit

for f in $F; do
  cp $f $f.strip
done

strip o/$m/blink/blink.strip
o/third_party/gcc/x86_64/bin/x86_64-linux-musl-strip o/$m/x86_64/blink/blink.strip
o/third_party/gcc/aarch64/bin/aarch64-linux-musl-strip o/$m/aarch64/blink/blink.strip
o/third_party/gcc/i486/bin/i486-linux-musl-strip o/$m/i486/blink/blink.strip

for f in $F; do
  ls -hal $f.strip
done
