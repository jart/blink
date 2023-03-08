# Qemu

This folder contains prebuilt binaries of Qemu 7.2.0-rc4. Qemu is
similar to Blink except it's bigger and it's been around longer. As
such, we've found Qemu enormously useful as a testing tool for Blink.
That's because Qemu is able to emulate multiple hardware architectures
from the comfort of an x86-64 workstation. That helps us quickly verify
our tinier, nimbler emulator for x86-64 binaries will work once shipped!

## Local Changes

None.

## License

Unlike Bilnk with observes the ISC license, Qemu is licensed GPL. You
have the freedom to obtain the Qemu source code from the internet and
reproduce our build process using the following instructions.

## Build Process

These binaries were compiled as follows:

```
cd ~/vendor/qemu-7.2.0-rc4
./configure --disable-system --enable-linux-user --disable-bsd-user --static \
--prefix=/opt/qemu-static-best --disable-stack-protector --enable-seccomp \
--disable-selinux --disable-glusterfs --disable-debug-info --cc=/usr/bin/gcc \
--extra-cflags='-fno-stack-protector' \
--extra-ldflags='-Wl,-z,norelro -Wl,-z,noseparate-code -Wl,-z,common-page-size=65536 -Wl,-z,max-page-size=65536' &&
make -j5 && make install
```

On the following system:

```
$ uname -a
Linux debian 6.1.0-rc4+ #2 SMP PREEMPT_DYNAMIC Thu Nov 10 20:53:21 PST 2022 x86_64 GNU/Linux
```

## Deploy Process

Our operations team used the following script to make these binaries
freely available on Google Cloud Storage.

```sh
#!/bin/sh
set -ex

T=/mnt/videos/website/justine.lol/qemu
Q=/opt/qemu-static-best
V=4
mkdir -p $T/$V
mkdir -p ~/blink/third_party/qemu/$V

for f in $(ls -1 $Q/bin | grep -v -- -system- | grep -v daemon); do

  cp $Q/bin/$f $T/$V/${f##*/}
  strip $T/$V/${f##*/}
  rm -f $T/$V/${f##*/}.gz
  gzip $T/$V/${f##*/}
  sha256sum -b $T/$V/${f##*/}.gz >$T/$V/${f##*/}.gz.sha256
  cp $T/$V/${f##*/}.gz.sha256 ~/blink/third_party/qemu/$V/${f##*/}.gz.sha256

  sed -i -e 's!/mnt/videos/website/justine.lol!third_party!' \
      ~/blink/third_party/qemu/$V/${f##*/}.gz.sha256

  ssh debian gsutil -m -h Cache-Control:public,max-age=31536000 cp -r -a public-read \
      /mnt/videos/website/justine.lol/qemu/$V/${f##*/}.gz \
      gs://justine/qemu/$V/
done

rm -rf /mnt/videos/website/justine.lol/qemu
```
