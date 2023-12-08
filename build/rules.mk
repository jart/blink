#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi ──────────────────────┘

o/$(MODE)/%.o: %.s
	@mkdir -p $(@D)
	$(AS) -o $@ $<

o/$(MODE)/%.o: %.S
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -c -o $@ $<

o/$(MODE)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/$(MODE)/%.h.ok: %.h
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -x c -g0 -o $@ $<

o/$(MODE)/%.a:
	rm -f $@
	$(AR) rcs $@ $^
o/$(MODE)/i486/%.a:
	rm -f $@
	$(VM) o/third_party/gcc/i486/bin/i486-linux-musl-ar rcsD $@ $^
o/$(MODE)/m68k/%.a:
	rm -f $@
	$(VM) o/third_party/gcc/m68k/bin/m68k-linux-musl-ar rcsD $@ $^
o/$(MODE)/x86_64/%.a:
	rm -f $@
	$(VM) o/third_party/gcc/x86_64/bin/x86_64-linux-musl-ar rcsD $@ $^
o/$(MODE)/x86_64-gcc49/%.a:
	rm -f $@
	$(VM) o/third_party/gcc/x86_64-gcc49/bin/x86_64-linux-musl-ar rcsD $@ $^
o/$(MODE)/arm/%.a:
	rm -f $@
	$(VM) o/third_party/gcc/arm/bin/arm-linux-musleabi-ar rcsD $@ $^
o/$(MODE)/aarch64/%.a:
	rm -f $@
	$(VM) o/third_party/gcc/aarch64/bin/aarch64-linux-musl-ar rcsD $@ $^
o/$(MODE)/riscv64/%.a:
	rm -f $@
	$(VM) o/third_party/gcc/riscv64/bin/riscv64-linux-musl-ar rcsD $@ $^
o/$(MODE)/mips/%.a:
	rm -f $@
	$(VM) o/third_party/gcc/mips/bin/mips-linux-musl-ar rcsD $@ $^
o/$(MODE)/mipsel/%.a:
	rm -f $@
	$(VM) o/third_party/gcc/mipsel/bin/mipsel-linux-musl-ar rcsD $@ $^
o/$(MODE)/mips64/%.a:
	rm -f $@
	$(VM) o/third_party/gcc/mips64/bin/mips64-linux-musl-ar rcsD $@ $^
o/$(MODE)/mips64el/%.a:
	rm -f $@
	$(VM) o/third_party/gcc/mips64el/bin/mips64el-linux-musl-ar rcsD $@ $^
o/$(MODE)/s390x/%.a:
	rm -f $@
	$(VM) o/third_party/gcc/s390x/bin/s390x-linux-musl-ar rcsD $@ $^
o/$(MODE)/microblaze/%.a:
	rm -f $@
	$(VM) o/third_party/gcc/microblaze/bin/microblaze-linux-musl-ar rcsD $@ $^
o/$(MODE)/powerpc/%.a:
	rm -f $@
	$(VM) o/third_party/gcc/powerpc/bin/powerpc-linux-musl-ar rcsD $@ $^
o/$(MODE)/powerpc64le/%.a:
	rm -f $@
	$(VM) o/third_party/gcc/powerpc64le/bin/powerpc64le-linux-musl-ar rcsD $@ $^

o/$(MODE)/%-gcc.asm: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector -iquote. $(CPPFLAGS) $(TARGET_ARCH) -S -g0 $(OUTPUT_OPTION) $<
o/$(MODE)/%-clang.asm: %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-stack-protector -iquote. $(CPPFLAGS) $(TARGET_ARCH) -S -g0 $(OUTPUT_OPTION) $<
