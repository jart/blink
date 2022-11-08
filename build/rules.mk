#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

o/$(MODE)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/$(MODE)/%.h.ok: %.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -x c -g0 -o $@ $<

o/$(MODE)/%.a:
	ar rcs $@ $^
o/$(MODE)/i486/%.a:
	o/third_party/gcc/i486/bin/i486-linux-musl-ar rcsD $@ $^
o/$(MODE)/m68k/%.a:
	o/third_party/gcc/m68k/bin/m68k-linux-musl-ar rcsD $@ $^
o/$(MODE)/x86_64/%.a:
	o/third_party/gcc/x86_64/bin/x86_64-linux-musl-ar rcsD $@ $^
o/$(MODE)/arm/%.a:
	o/third_party/gcc/arm/bin/arm-linux-musleabi-ar rcsD $@ $^
o/$(MODE)/aarch64/%.a:
	o/third_party/gcc/aarch64/bin/aarch64-linux-musl-ar rcsD $@ $^
o/$(MODE)/riscv64/%.a:
	o/third_party/gcc/riscv64/bin/riscv64-linux-musl-ar rcsD $@ $^
o/$(MODE)/mips/%.a:
	o/third_party/gcc/mips/bin/mips-linux-musl-ar rcsD $@ $^
o/$(MODE)/mipsel/%.a:
	o/third_party/gcc/mipsel/bin/mipsel-linux-musl-ar rcsD $@ $^
o/$(MODE)/mips64/%.a:
	o/third_party/gcc/mips64/bin/mips64-linux-musl-ar rcsD $@ $^
o/$(MODE)/mips64el/%.a:
	o/third_party/gcc/mips64el/bin/mips64el-linux-musl-ar rcsD $@ $^
o/$(MODE)/s390x/%.a:
	o/third_party/gcc/s390x/bin/s390x-linux-musl-ar rcsD $@ $^
o/$(MODE)/microblaze/%.a:
	o/third_party/gcc/microblaze/bin/microblaze-linux-musl-ar rcsD $@ $^
o/$(MODE)/powerpc/%.a:
	o/third_party/gcc/powerpc/bin/powerpc-linux-musl-ar rcsD $@ $^
o/$(MODE)/powerpc64le/%.a:
	o/third_party/gcc/powerpc64le/bin/powerpc64le-linux-musl-ar rcsD $@ $^
