#!/bin/sh
m=tiny

echo -n 'did you update blink/tunables.h and ape/ape.s? [n] '
read didit
if [ "$didit" != "y" ]; then
  exit 1
fi

set -ex
./configure
make -j8 check

# create the linux arm64 build
make m=$m -j8 o/$m/aarch64/blink/blink
o/third_party/gcc/aarch64/bin/aarch64-linux-musl-strip \
  o/$m/aarch64/blink/blink
gzip -c9 o/$m/aarch64/blink/blink \
     >o/$m/aarch64/blink/blink-aarch64.gz
cp o/$m/aarch64/blink/blink-aarch64.gz \
   ~/cosmo/ape/blink-aarch64.gz

# create the apple arm64 build
rsync -avz --exclude=o \
      --exclude='*.xz' \
      --exclude='*.gz' \
      --exclude='*.dbg' \
      --exclude='*.com' \
      --exclude='*.dbg' \
      --exclude='*.elf' \
      ~/blink silicon:
ssh silicon cd blink \&\& \
    /usr/local/bin/make m=$m -j8 o/$m/blink/blink \; \
    /usr/local/bin/make m=$m -j8 o/$m/blink/blink \&\& \
    strip o/$m/blink/blink \&\& \
    gzip -c9 o/$m/blink/blink \>o/$m/blink/blink.gz
scp silicon:blink/o/$m/blink/blink.gz \
    /home/jart/cosmo/ape/blink-darwin-arm64.gz

# smoke test the release
(
  cd ~/cosmo
  make -j8 o//test/libc/thread/pthread_create_test.com
)
scp ~/cosmo/o//test/libc/thread/pthread_create_test.com silicon:
ssh silicon ./pthread_create_test.com
scp ~/cosmo/o//test/libc/thread/pthread_create_test.com pi:
ssh pi ./pthread_create_test.com

# we're finished
o//blink/blink -v
ls -hal \
   ~/cosmo/ape/blink-aarch64.gz \
   ~/cosmo/ape/blink-darwin-arm64.gz
