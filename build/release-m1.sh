#!/bin/sh
set -ex
m=
make m=$m -j8 o/$m/blink/blink
cp o/$m/blink/blink o/$m/blink/blink-darwin-arm64
strip o/$m/blink/blink-darwin-arm64
gzip -c9 o/$m/blink/blink-darwin-arm64 \
     >o/$m/blink/blink-darwin-arm64.gz
ls -hal o/$m/blink/blink-darwin-arm64.gz
echo
echo justine justine now run
echo scp silicon:blink/o/$m/blink/blink-darwin-arm64.gz /home/jart/cosmo/ape
