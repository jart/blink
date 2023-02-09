# Libc Test Project

This folder downloads prebuilt binaries from the [Libc Test
Project](https://github.com/jart/libc-test). This is Musl Libc's test
suite collection.

## Origin

git://repo.or.cz/libc-test

## Changes

See link above to our GitHub fork.

## Build

The binaries were compiled on Alpine Linux using:

```
export CC="cc -fno-omit-frame-pointer -Wl,-z,common-page-size=65536,-z,max-page-size=65536"
make -j16
make install
```

## Deploy

Our operations team them uses [deploy-libc-test](deploy-libc-test) to
deploy individual test programs to Google Cloud Storage once they've
been confirmed to work.
