#!/bin/sh

F='
o/tiny/blink/blink
o/tiny/x86_64/blink/blink
o/tiny/aarch64/blink/blink
'

make -j8 $F m=tiny || exit

for f in $F; do
  cp $f $f.strip
done

strip o/tiny/blink/blink.strip
o/third_party/gcc/x86_64/bin/x86_64-linux-musl-strip o/tiny/x86_64/blink/blink.strip
o/third_party/gcc/aarch64/bin/aarch64-linux-musl-strip o/tiny/aarch64/blink/blink.strip

for f in $F; do
  ls -hal $f.strip
done
