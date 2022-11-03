#!/bin/sh
set -ex

make MODE=tiny -j8 test

o/third_party/gcc/aarch64/bin/aarch64-linux-musl-strip o/tiny/aarch64/blink/blink
o/third_party/gcc/arm/bin/arm-linux-musleabi-strip o/tiny/arm/blink/blink
o/third_party/gcc/i486/bin/i486-linux-musl-strip o/tiny/i486/blink/blink
o/third_party/gcc/m68k/bin/m68k-linux-musl-strip o/tiny/m68k/blink/blink
o/third_party/gcc/microblaze/bin/microblaze-linux-musl-strip o/tiny/microblaze/blink/blink
o/third_party/gcc/riscv64/bin/riscv64-linux-musl-strip o/tiny/riscv64/blink/blink
o/third_party/gcc/mips/bin/mips-linux-musl-strip o/tiny/mips/blink/blink
o/third_party/gcc/mipsel/bin/mipsel-linux-musl-strip o/tiny/mipsel/blink/blink
o/third_party/gcc/mips64/bin/mips64-linux-musl-strip o/tiny/mips64/blink/blink
o/third_party/gcc/mips64el/bin/mips64el-linux-musl-strip o/tiny/mips64el/blink/blink
o/third_party/gcc/powerpc/bin/powerpc-linux-musl-strip o/tiny/powerpc/blink/blink
o/third_party/gcc/powerpc64le/bin/powerpc64le-linux-musl-strip o/tiny/powerpc64le/blink/blink
o/third_party/gcc/s390x/bin/s390x-linux-musl-strip o/tiny/s390x/blink/blink
o/third_party/gcc/x86_64/bin/x86_64-linux-musl-strip o/tiny/x86_64/blink/blink

gzip <o/tiny/aarch64/blink/blink >o/tiny/aarch64/blink/blink.gz
gzip <o/tiny/arm/blink/blink >o/tiny/arm/blink/blink.gz
gzip <o/tiny/i486/blink/blink >o/tiny/i486/blink/blink.gz
gzip <o/tiny/m68k/blink/blink >o/tiny/m68k/blink/blink.gz
gzip <o/tiny/microblaze/blink/blink >o/tiny/microblaze/blink/blink.gz
gzip <o/tiny/riscv64/blink/blink >o/tiny/riscv64/blink/blink.gz
gzip <o/tiny/mips/blink/blink >o/tiny/mips/blink/blink.gz
gzip <o/tiny/mipsel/blink/blink >o/tiny/mipsel/blink/blink.gz
gzip <o/tiny/mips64/blink/blink >o/tiny/mips64/blink/blink.gz
gzip <o/tiny/mips64el/blink/blink >o/tiny/mips64el/blink/blink.gz
gzip <o/tiny/powerpc/blink/blink >o/tiny/powerpc/blink/blink.gz
gzip <o/tiny/powerpc64le/blink/blink >o/tiny/powerpc64le/blink/blink.gz
gzip <o/tiny/s390x/blink/blink >o/tiny/s390x/blink/blink.gz
gzip <o/tiny/x86_64/blink/blink >o/tiny/x86_64/blink/blink.gz

xz <o/tiny/aarch64/blink/blink >o/tiny/aarch64/blink/blink.xz
xz <o/tiny/arm/blink/blink >o/tiny/arm/blink/blink.xz
xz <o/tiny/i486/blink/blink >o/tiny/i486/blink/blink.xz
xz <o/tiny/m68k/blink/blink >o/tiny/m68k/blink/blink.xz
xz <o/tiny/microblaze/blink/blink >o/tiny/microblaze/blink/blink.xz
xz <o/tiny/riscv64/blink/blink >o/tiny/riscv64/blink/blink.xz
xz <o/tiny/mips/blink/blink >o/tiny/mips/blink/blink.xz
xz <o/tiny/mipsel/blink/blink >o/tiny/mipsel/blink/blink.xz
xz <o/tiny/mips64/blink/blink >o/tiny/mips64/blink/blink.xz
xz <o/tiny/mips64el/blink/blink >o/tiny/mips64el/blink/blink.xz
xz <o/tiny/powerpc/blink/blink >o/tiny/powerpc/blink/blink.xz
xz <o/tiny/powerpc64le/blink/blink >o/tiny/powerpc64le/blink/blink.xz
xz <o/tiny/s390x/blink/blink >o/tiny/s390x/blink/blink.xz
xz <o/tiny/x86_64/blink/blink >o/tiny/x86_64/blink/blink.xz

ls -hal \
   o/tiny/aarch64/blink/blink \
   o/tiny/arm/blink/blink \
   o/tiny/i486/blink/blink \
   o/tiny/m68k/blink/blink \
   o/tiny/microblaze/blink/blink \
   o/tiny/riscv64/blink/blink \
   o/tiny/mips/blink/blink \
   o/tiny/mipsel/blink/blink \
   o/tiny/mips64/blink/blink \
   o/tiny/mips64el/blink/blink \
   o/tiny/powerpc/blink/blink \
   o/tiny/powerpc64le/blink/blink \
   o/tiny/s390x/blink/blink \
   o/tiny/x86_64/blink/blink

ls -hal \
   o/tiny/aarch64/blink/blink.gz \
   o/tiny/arm/blink/blink.gz \
   o/tiny/i486/blink/blink.gz \
   o/tiny/m68k/blink/blink.gz \
   o/tiny/microblaze/blink/blink.gz \
   o/tiny/riscv64/blink/blink.gz \
   o/tiny/mips/blink/blink.gz \
   o/tiny/mipsel/blink/blink.gz \
   o/tiny/mips64/blink/blink.gz \
   o/tiny/mips64el/blink/blink.gz \
   o/tiny/powerpc/blink/blink.gz \
   o/tiny/powerpc64le/blink/blink.gz \
   o/tiny/s390x/blink/blink.gz \
   o/tiny/x86_64/blink/blink.gz

ls -hal \
   o/tiny/aarch64/blink/blink.xz \
   o/tiny/arm/blink/blink.xz \
   o/tiny/i486/blink/blink.xz \
   o/tiny/m68k/blink/blink.xz \
   o/tiny/microblaze/blink/blink.xz \
   o/tiny/riscv64/blink/blink.xz \
   o/tiny/mips/blink/blink.xz \
   o/tiny/mipsel/blink/blink.xz \
   o/tiny/mips64/blink/blink.xz \
   o/tiny/mips64el/blink/blink.xz \
   o/tiny/powerpc/blink/blink.xz \
   o/tiny/powerpc64le/blink/blink.xz \
   o/tiny/s390x/blink/blink.xz \
   o/tiny/x86_64/blink/blink.xz

cp o/tiny/aarch64/blink/blink.gz ~/cosmo/ape/blink-aarch64.gz
cp o/tiny/arm/blink/blink.gz ~/cosmo/ape/blink-arm.gz
cp o/tiny/i486/blink/blink.gz ~/cosmo/ape/blink-i486.gz
cp o/tiny/m68k/blink/blink.gz ~/cosmo/ape/blink-m68k.gz
cp o/tiny/riscv64/blink/blink.gz ~/cosmo/ape/blink-riscv64.gz
cp o/tiny/mips/blink/blink.gz ~/cosmo/ape/blink-mips.gz
cp o/tiny/mipsel/blink/blink.gz ~/cosmo/ape/blink-mipsel.gz
cp o/tiny/mips64/blink/blink.gz ~/cosmo/ape/blink-mips64.gz
cp o/tiny/mips64el/blink/blink.gz ~/cosmo/ape/blink-mips64el.gz
cp o/tiny/powerpc/blink/blink.gz ~/cosmo/ape/blink-powerpc.gz
cp o/tiny/powerpc64le/blink/blink.gz ~/cosmo/ape/blink-powerpc64le.gz
cp o/tiny/s390x/blink/blink.gz ~/cosmo/ape/blink-s390x.gz
