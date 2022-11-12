#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

o/$(MODE)/%.runs: o/$(MODE)/%
	$<
	@touch $@

o/$(MODE)/%.ok: % o/$(MODE)/blink/blink
	@mkdir -p $(@D)
	o/$(MODE)/blink/blink $<
	@touch $@

o/$(MODE)/i486/%.runs: o/$(MODE)/i486/% o/third_party/qemu/qemu-i486
	o/third_party/qemu/qemu-i486 $<
	@touch $@
o/$(MODE)/m68k/%.runs: o/$(MODE)/m68k/% o/third_party/qemu/qemu-m68k
	o/third_party/qemu/qemu-m68k $<
	@touch $@
o/$(MODE)/x86_64/%.runs: o/$(MODE)/x86_64/% o/third_party/qemu/qemu-x86_64
	o/third_party/qemu/qemu-x86_64 $<
	@touch $@
o/$(MODE)/arm/%.runs: o/$(MODE)/arm/% o/third_party/qemu/qemu-arm
	o/third_party/qemu/qemu-arm $<
	@touch $@
o/$(MODE)/aarch64/%.runs: o/$(MODE)/aarch64/% o/third_party/qemu/qemu-aarch64
	o/third_party/qemu/qemu-aarch64 $<
	@touch $@
o/$(MODE)/riscv64/%.runs: o/$(MODE)/riscv64/% o/third_party/qemu/qemu-riscv64
	o/third_party/qemu/qemu-riscv64 $<
	@touch $@
o/$(MODE)/mips/%.runs: o/$(MODE)/mips/% o/third_party/qemu/qemu-mips
	o/third_party/qemu/qemu-mips $<
	@touch $@
o/$(MODE)/mipsel/%.runs: o/$(MODE)/mipsel/% o/third_party/qemu/qemu-mipsel
	o/third_party/qemu/qemu-mipsel $<
	@touch $@
o/$(MODE)/mips64/%.runs: o/$(MODE)/mips64/% o/third_party/qemu/qemu-mips64
	o/third_party/qemu/qemu-mips64 $<
	@touch $@
o/$(MODE)/mips64el/%.runs: o/$(MODE)/mips64el/% o/third_party/qemu/qemu-mips64el
	o/third_party/qemu/qemu-mips64el $<
	@touch $@
o/$(MODE)/s390x/%.runs: o/$(MODE)/s390x/% o/third_party/qemu/qemu-s390x
	o/third_party/qemu/qemu-s390x $<
	@touch $@
o/$(MODE)/microblaze/%.runs: o/$(MODE)/microblaze/% o/third_party/qemu/qemu-microblaze
	o/third_party/qemu/qemu-microblaze $<
	@touch $@
o/$(MODE)/powerpc/%.runs: o/$(MODE)/powerpc/% o/third_party/qemu/qemu-powerpc
	o/third_party/qemu/qemu-powerpc $<
	@touch $@
o/$(MODE)/powerpc64le/%.runs: o/$(MODE)/powerpc64le/% o/third_party/qemu/qemu-powerpc64le
	o/third_party/qemu/qemu-powerpc64le $<
	@touch $@

o/$(MODE)/i486/%.emulates: % o/i486/blink/blink o/third_party/qemu/qemu-i486
	@mkdir -p $(@D)
	o/third_party/qemu/qemu-i486 o/i486/blink/blink $<
	@touch $@
o/$(MODE)/m68k/%.emulates: % o/m68k/blink/blink o/third_party/qemu/qemu-m68k
	@mkdir -p $(@D)
	o/third_party/qemu/qemu-m68k o/m68k/blink/blink $<
	@touch $@
o/$(MODE)/x86_64/%.emulates: % o/x86_64/blink/blink o/third_party/qemu/qemu-x86_64
	@mkdir -p $(@D)
	o/third_party/qemu/qemu-x86_64 o/x86_64/blink/blink $<
	@touch $@
o/$(MODE)/arm/%.emulates: % o/arm/blink/blink o/third_party/qemu/qemu-arm
	@mkdir -p $(@D)
	o/third_party/qemu/qemu-arm o/arm/blink/blink $<
	@touch $@
o/$(MODE)/aarch64/%.emulates: % o/aarch64/blink/blink o/third_party/qemu/qemu-aarch64
	@mkdir -p $(@D)
	o/third_party/qemu/qemu-aarch64 o/aarch64/blink/blink $<
	@touch $@
o/$(MODE)/riscv64/%.emulates: % o/riscv64/blink/blink o/third_party/qemu/qemu-riscv64
	@mkdir -p $(@D)
	o/third_party/qemu/qemu-riscv64 o/riscv64/blink/blink $<
	@touch $@
o/$(MODE)/mips/%.emulates: % o/mips/blink/blink o/third_party/qemu/qemu-mips
	@mkdir -p $(@D)
	o/third_party/qemu/qemu-mips o/mips/blink/blink $<
	@touch $@
o/$(MODE)/mipsel/%.emulates: % o/mipsel/blink/blink o/third_party/qemu/qemu-mipsel
	@mkdir -p $(@D)
	o/third_party/qemu/qemu-mipsel o/mipsel/blink/blink $<
	@touch $@
o/$(MODE)/mips64/%.emulates: % o/mips64/blink/blink o/third_party/qemu/qemu-mips64
	@mkdir -p $(@D)
	o/third_party/qemu/qemu-mips64 o/mips64/blink/blink $<
	@touch $@
o/$(MODE)/mips64el/%.emulates: % o/mips64el/blink/blink o/third_party/qemu/qemu-mips64el
	@mkdir -p $(@D)
	o/third_party/qemu/qemu-mips64el o/mips64el/blink/blink $<
	@touch $@
o/$(MODE)/s390x/%.emulates: % o/s390x/blink/blink o/third_party/qemu/qemu-s390x
	@mkdir -p $(@D)
	o/third_party/qemu/qemu-s390x o/s390x/blink/blink $<
	@touch $@
o/$(MODE)/microblaze/%.emulates: % o/microblaze/blink/blink o/third_party/qemu/qemu-microblaze
	@mkdir -p $(@D)
	o/third_party/qemu/qemu-microblaze o/microblaze/blink/blink $<
	@touch $@
o/$(MODE)/powerpc/%.emulates: % o/powerpc/blink/blink o/third_party/qemu/qemu-powerpc
	@mkdir -p $(@D)
	o/third_party/qemu/qemu-powerpc o/powerpc/blink/blink $<
	@touch $@
o/$(MODE)/powerpc64le/%.emulates: % o/powerpc64le/blink/blink o/third_party/qemu/qemu-powerpc64le
	@mkdir -p $(@D)
	o/third_party/qemu/qemu-powerpc64le o/powerpc64le/blink/blink $<
	@touch $@

o/$(MODE)/test:				\
	o/$(MODE)/test/blink

o/$(MODE)/test/emulates:		\
	o/$(MODE)/test/blink/emulates
