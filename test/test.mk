#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

o/$(MODE)/%.runs: o/$(MODE)/%
	$<
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

o/$(MODE)/test: o/$(MODE)/test/blink
