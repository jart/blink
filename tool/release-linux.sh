#!/bin/bash
set -ex
v=1.1

d=/mnt/videos/blink-$v
[ ! -d $d ]
mkdir -p $d

./configure --static --enable-vfs

make -j32 \
o//x86_64/blink/blink \
o//x86_64/blink/blinkenlights \
o//aarch64/blink/blink \
o//aarch64/blink/blinkenlights \
o//arm/blink/blink \
o//arm/blink/blinkenlights \
o//i486/blink/blink \
o//i486/blink/blinkenlights \
o//mips/blink/blink \
o//mips/blink/blinkenlights \
o//mipsel/blink/blink \
o//mipsel/blink/blinkenlights \
o//mips64/blink/blink \
o//mips64/blink/blinkenlights \
o//mips64el/blink/blink \
o//mips64el/blink/blinkenlights \
o//powerpc/blink/blink \
o//powerpc/blink/blinkenlights \
o//powerpc64le/blink/blink \
o//powerpc64le/blink/blinkenlights \
o//s390x/blink/blink \
o//s390x/blink/blinkenlights

pdf() {
  groff -Tps -man $1 >$2.ps
  ps2pdf $2.ps $2.pdf
}

git archive --format=tar.gz -o $d/blink-$v.tar.gz --prefix=blink-$v/ master
pdf blink/blink.1 $d/blink-$v
pdf blink/blinkenlights.1 $d/blinkenlights-$v
gzip -9 <o//x86_64/blink/blink >$d/blink-$v-linux-x86_64.elf.gz
gzip -9 <o//x86_64/blink/blinkenlights >$d/blinkenlights-$v-linux-x86_64.elf.gz
gzip -9 <o//aarch64/blink/blink >$d/blink-$v-linux-aarch64.elf.gz
gzip -9 <o//aarch64/blink/blinkenlights >$d/blinkenlights-$v-linux-aarch64.elf.gz
gzip -9 <o//arm/blink/blink >$d/blink-$v-linux-arm.elf.gz
gzip -9 <o//arm/blink/blinkenlights >$d/blinkenlights-$v-linux-arm.elf.gz
gzip -9 <o//i486/blink/blink >$d/blink-$v-linux-i486.elf.gz
gzip -9 <o//i486/blink/blinkenlights >$d/blinkenlights-$v-linux-i486.elf.gz
gzip -9 <o//mips/blink/blink >$d/blink-$v-linux-mips.elf.gz
gzip -9 <o//mips/blink/blinkenlights >$d/blinkenlights-$v-linux-mips.elf.gz
gzip -9 <o//mipsel/blink/blink >$d/blink-$v-linux-mipsel.elf.gz
gzip -9 <o//mipsel/blink/blinkenlights >$d/blinkenlights-$v-linux-mipsel.elf.gz
gzip -9 <o//mips64/blink/blink >$d/blink-$v-linux-mips64.elf.gz
gzip -9 <o//mips64/blink/blinkenlights >$d/blinkenlights-$v-linux-mips64.elf.gz
gzip -9 <o//mips64el/blink/blink >$d/blink-$v-linux-mips64el.elf.gz
gzip -9 <o//mips64el/blink/blinkenlights >$d/blinkenlights-$v-linux-mips64el.elf.gz
gzip -9 <o//powerpc/blink/blink >$d/blink-$v-linux-powerpc.elf.gz
gzip -9 <o//powerpc/blink/blinkenlights >$d/blinkenlights-$v-linux-powerpc.elf.gz
gzip -9 <o//powerpc64le/blink/blink >$d/blink-$v-linux-powerpc64le.elf.gz
gzip -9 <o//powerpc64le/blink/blinkenlights >$d/blinkenlights-$v-linux-powerpc64le.elf.gz
gzip -9 <o//s390x/blink/blink >$d/blink-$v-linux-s390x.elf.gz
gzip -9 <o//s390x/blink/blinkenlights >$d/blinkenlights-$v-linux-s390x.elf.gz
