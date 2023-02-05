# Linux Test Project

This folder downloads prebuilt binaries from the [Linux Test
Project](https://github.com/linux-test-project/ltp).

## Changes

1. Comment out the capability code in lib/tst_capability.c
2. Comment out the code that opens /proc/meminfo
3. Remove `clone()` + `O_PATH` test from `close_range02`

## Build

The binaries were compiled on Alpine Linux using:

```
./configure CFLAGS="-Os -fno-omit-frame-pointer" \
            LDFLAGS="-Wl,-z,common-page-size=65536,-z,max-page-size=65536"
make -j16
make install
```

## Deploy

The above commands install to `/opt/ltp` by default. Our operations team
them uses [deploy-ltp](deploy-ltp) to deploy individual test programs to
Google Cloud Storage once they've been confirmed to work.
