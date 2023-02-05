# Musl Libc

This folder downloads prebuilt binaries from the [Musl
Libc](https://www.musl-libc.org/) project.

## Changes

None.

## Build

The binaries were compiled on Alpine Linux using:

```
export CC="cc -fno-omit-frame-pointer -Wl,-z,common-page-size=65536,-z,max-page-size=65536"
./configure --enable-debug
make -j16
```
