#!/bin/sh
m=${1:-tiny}

F="
o/$m/x86_64/blink/blink
"

make -j8 $F m=$m || exit

for f in $F; do
  cp $f $f.strip
done

o/third_party/gcc/x86_64/bin/x86_64-linux-musl-strip o/$m/x86_64/blink/blink.strip

for f in $F; do
  ls -hal $f.strip
done
