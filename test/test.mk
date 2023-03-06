#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

emulates = o/$(MODE)/$1.ok $(foreach ARCH,$(ARCHITECTURES),o/$(MODE)/$(ARCH)/$1.emulates)

# make -j8 o//test/alu
# very time consuming tests of our core arithmetic ops
# https://github.com/jart/cosmopolitan/blob/master/test/tool/build/lib/optest.c
# https://github.com/jart/cosmopolitan/blob/master/test/tool/build/lib/alu_test.c
# https://github.com/jart/cosmopolitan/blob/master/test/tool/build/lib/bsu_test.c
o/$(MODE)/test/alu:								\
		$(call emulates,third_party/cosmo/2/alu_test.com)		\
		$(call emulates,third_party/cosmo/2/bsu_test.com)
	@mkdir -p $(@D)
	@touch $@

# make -j8 o//test/sse
# fast and fairly comprehensive tests for our simd instructions
# https://github.com/jart/cosmopolitan/blob/master/test/libc/intrin/intrin_test.c
o/$(MODE)/test/sse:								\
		$(call emulates,third_party/cosmo/7/intrin_test.com)		\
		$(call emulates,third_party/cosmo/2/popcnt_test.com)		\
		$(call emulates,third_party/cosmo/2/pshuf_test.com)		\
		$(call emulates,third_party/cosmo/2/pmulhrsw_test.com)		\
		$(call emulates,third_party/cosmo/2/divmul_test.com)		\
		$(call emulates,third_party/cosmo/2/palignr_test.com)
	@mkdir -p $(@D)
	@touch $@

# make -j8 o//test/lib
# test some c libraries
o/$(MODE)/test/lib:								\
		$(call emulates,third_party/cosmo/2/atoi_test.com)		\
		$(call emulates,third_party/cosmo/2/qsort_test.com)		\
		$(call emulates,third_party/cosmo/2/kprintf_test.com)
	@mkdir -p $(@D)
	@touch $@

# make -j8 o//test/sys
# test linux system call emulation
o/$(MODE)/test/sys:								\
		$(call emulates,third_party/cosmo/2/renameat_test.com)		\
		$(call emulates,third_party/cosmo/2/clock_gettime_test.com)	\
		$(call emulates,third_party/cosmo/2/clock_getres_test.com)	\
		$(call emulates,third_party/cosmo/2/clock_nanosleep_test.com)	\
		$(call emulates,third_party/cosmo/2/access_test.com)		\
		$(call emulates,third_party/cosmo/2/chdir_test.com)		\
		$(call emulates,third_party/cosmo/2/closefrom_test.com)		\
		$(call emulates,third_party/cosmo/2/commandv_test.com)		\
		$(call emulates,third_party/cosmo/2/dirstream_test.com)		\
		$(call emulates,third_party/cosmo/2/dirname_test.com)		\
		$(call emulates,third_party/cosmo/2/clone_test.com)		\
		$(call emulates,third_party/cosmo/2/fileexists_test.com)	\
		$(call emulates,third_party/cosmo/2/getcwd_test.com)		\
		$(call emulates,third_party/cosmo/2/lseek_test.com)		\
		$(call emulates,third_party/cosmo/2/mkdir_test.com)		\
		$(call emulates,third_party/cosmo/2/makedirs_test.com)		\
		$(call emulates,third_party/cosmo/2/nanosleep_test.com)		\
		$(call emulates,third_party/cosmo/2/readlinkat_test.com)	\
		$(call emulates,third_party/cosmo/2/symlinkat_test.com)		\
		$(call emulates,third_party/cosmo/2/tls_test.com)		\
		$(call emulates,third_party/cosmo/2/tmpfile_test.com)		\
		$(call emulates,third_party/cosmo/2/xslurp_test.com)
	@mkdir -p $(@D)
	@touch $@

o/$(MODE)/%.runs: o/$(MODE)/%
	$<
	@touch $@

o/$(MODE)/%.ok: % o/$(MODE)/blink/blink
	@mkdir -p $(@D)
	o/$(MODE)/blink/blink $<
	@touch $@

o/$(MODE)/i486/%.runs: o/$(MODE)/i486/% o/third_party/qemu/qemu-i486 $(VM)
	$(VM) o/third_party/qemu/qemu-i486 $<
	@touch $@
o/$(MODE)/m68k/%.runs: o/$(MODE)/m68k/% o/third_party/qemu/qemu-m68k $(VM)
	$(VM) o/third_party/qemu/qemu-m68k $<
	@touch $@
o/$(MODE)/x86_64/%.runs: o/$(MODE)/x86_64/% o/third_party/qemu/qemu-x86_64 $(VM)
	$(VM) o/third_party/qemu/qemu-x86_64 -cpu core2duo $<
	@touch $@
o/$(MODE)/x86_64-gcc49/%.runs: o/$(MODE)/x86_64-gcc49/% o/third_party/qemu/qemu-x86_64 $(VM)
	$(VM) o/third_party/qemu/qemu-x86_64 -cpu core2duo $<
	@touch $@
o/$(MODE)/arm/%.runs: o/$(MODE)/arm/% o/third_party/qemu/qemu-arm $(VM)
	$(VM) o/third_party/qemu/qemu-arm $<
	@touch $@
o/$(MODE)/aarch64/%.runs: o/$(MODE)/aarch64/% o/third_party/qemu/qemu-aarch64 $(VM)
	$(VM) o/third_party/qemu/qemu-aarch64 $<
	@touch $@
o/$(MODE)/riscv64/%.runs: o/$(MODE)/riscv64/% o/third_party/qemu/qemu-riscv64 $(VM)
	$(VM) o/third_party/qemu/qemu-riscv64 $<
	@touch $@
o/$(MODE)/mips/%.runs: o/$(MODE)/mips/% o/third_party/qemu/qemu-mips $(VM)
	$(VM) o/third_party/qemu/qemu-mips $<
	@touch $@
o/$(MODE)/mipsel/%.runs: o/$(MODE)/mipsel/% o/third_party/qemu/qemu-mipsel $(VM)
	$(VM) o/third_party/qemu/qemu-mipsel $<
	@touch $@
o/$(MODE)/mips64/%.runs: o/$(MODE)/mips64/% o/third_party/qemu/qemu-mips64 $(VM)
	$(VM) o/third_party/qemu/qemu-mips64 $<
	@touch $@
o/$(MODE)/mips64el/%.runs: o/$(MODE)/mips64el/% o/third_party/qemu/qemu-mips64el $(VM)
	$(VM) o/third_party/qemu/qemu-mips64el $<
	@touch $@
o/$(MODE)/s390x/%.runs: o/$(MODE)/s390x/% o/third_party/qemu/qemu-s390x $(VM)
	$(VM) o/third_party/qemu/qemu-s390x $<
	@touch $@
o/$(MODE)/microblaze/%.runs: o/$(MODE)/microblaze/% o/third_party/qemu/qemu-microblaze $(VM)
	$(VM) o/third_party/qemu/qemu-microblaze $<
	@touch $@
o/$(MODE)/powerpc/%.runs: o/$(MODE)/powerpc/% o/third_party/qemu/qemu-powerpc $(VM)
	$(VM) o/third_party/qemu/qemu-powerpc $<
	@touch $@
o/$(MODE)/powerpc64le/%.runs: o/$(MODE)/powerpc64le/% o/third_party/qemu/qemu-powerpc64le $(VM)
	$(VM) o/third_party/qemu/qemu-powerpc64le $<
	@touch $@

o/$(MODE)/i486/%.emulates: % o/$(MODE)/i486/blink/blink o/third_party/qemu/qemu-i486 $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-i486 o/$(MODE)/i486/blink/blink $<
	@touch $@
o/$(MODE)/m68k/%.emulates: % o/$(MODE)/m68k/blink/blink o/third_party/qemu/qemu-m68k $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-m68k o/$(MODE)/m68k/blink/blink $<
	@touch $@
o/$(MODE)/x86_64/%.emulates: % o/$(MODE)/x86_64/blink/blink o/third_party/qemu/qemu-x86_64 $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-x86_64 -cpu core2duo o/$(MODE)/x86_64/blink/blink $<
	@touch $@
o/$(MODE)/x86_64-gcc49/%.emulates: % o/$(MODE)/x86_64-gcc49/blink/blink o/third_party/qemu/qemu-x86_64 $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-x86_64 -cpu core2duo o/$(MODE)/x86_64-gcc49/blink/blink $<
	@touch $@
o/$(MODE)/arm/%.emulates: % o/$(MODE)/arm/blink/blink o/third_party/qemu/qemu-arm $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-arm o/$(MODE)/arm/blink/blink $<
	@touch $@
o/$(MODE)/aarch64/%.emulates: % o/$(MODE)/aarch64/blink/blink o/third_party/qemu/qemu-aarch64 $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-aarch64 o/$(MODE)/aarch64/blink/blink $<
	@touch $@
o/$(MODE)/riscv64/%.emulates: % o/$(MODE)/riscv64/blink/blink o/third_party/qemu/qemu-riscv64 $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-riscv64 o/$(MODE)/riscv64/blink/blink $<
	@touch $@
o/$(MODE)/mips/%.emulates: % o/$(MODE)/mips/blink/blink o/third_party/qemu/qemu-mips $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-mips o/$(MODE)/mips/blink/blink $<
	@touch $@
o/$(MODE)/mipsel/%.emulates: % o/$(MODE)/mipsel/blink/blink o/third_party/qemu/qemu-mipsel $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-mipsel o/$(MODE)/mipsel/blink/blink $<
	@touch $@
o/$(MODE)/mips64/%.emulates: % o/$(MODE)/mips64/blink/blink o/third_party/qemu/qemu-mips64 $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-mips64 o/$(MODE)/mips64/blink/blink $<
	@touch $@
o/$(MODE)/mips64el/%.emulates: % o/$(MODE)/mips64el/blink/blink o/third_party/qemu/qemu-mips64el $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-mips64el o/$(MODE)/mips64el/blink/blink $<
	@touch $@
o/$(MODE)/s390x/%.emulates: % o/$(MODE)/s390x/blink/blink o/third_party/qemu/qemu-s390x $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-s390x o/$(MODE)/s390x/blink/blink $<
	@touch $@
o/$(MODE)/microblaze/%.emulates: % o/$(MODE)/microblaze/blink/blink o/third_party/qemu/qemu-microblaze $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-microblaze o/$(MODE)/microblaze/blink/blink $<
	@touch $@
o/$(MODE)/powerpc/%.emulates: % o/$(MODE)/powerpc/blink/blink o/third_party/qemu/qemu-powerpc $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-powerpc o/$(MODE)/powerpc/blink/blink $<
	@touch $@
o/$(MODE)/powerpc64le/%.emulates: % o/$(MODE)/powerpc64le/blink/blink o/third_party/qemu/qemu-powerpc64le $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-powerpc64le o/$(MODE)/powerpc64le/blink/blink $<
	@touch $@

o/$(MODE)/i486/%.emulates: o/$(MODE)/% o/$(MODE)/i486/blink/blink o/third_party/qemu/qemu-i486 $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-i486 o/$(MODE)/i486/blink/blink $<
	@touch $@
o/$(MODE)/m68k/%.emulates: o/$(MODE)/% o/$(MODE)/m68k/blink/blink o/third_party/qemu/qemu-m68k $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-m68k o/$(MODE)/m68k/blink/blink $<
	@touch $@
o/$(MODE)/x86_64/%.emulates: o/$(MODE)/% o/$(MODE)/x86_64/blink/blink o/third_party/qemu/qemu-x86_64 $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-x86_64 -cpu core2duo o/$(MODE)/x86_64/blink/blink $<
	@touch $@
o/$(MODE)/x86_64-gcc49/%.emulates: o/$(MODE)/% o/$(MODE)/x86_64-gcc49/blink/blink o/third_party/qemu/qemu-x86_64 $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-x86_64 -cpu core2duo o/$(MODE)/x86_64-gcc49/blink/blink $<
	@touch $@
o/$(MODE)/arm/%.emulates: o/$(MODE)/% o/$(MODE)/arm/blink/blink o/third_party/qemu/qemu-arm $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-arm o/$(MODE)/arm/blink/blink $<
	@touch $@
o/$(MODE)/aarch64/%.emulates: o/$(MODE)/% o/$(MODE)/aarch64/blink/blink o/third_party/qemu/qemu-aarch64 $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-aarch64 o/$(MODE)/aarch64/blink/blink $<
	@touch $@
o/$(MODE)/riscv64/%.emulates: o/$(MODE)/% o/$(MODE)/riscv64/blink/blink o/third_party/qemu/qemu-riscv64 $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-riscv64 o/$(MODE)/riscv64/blink/blink $<
	@touch $@
o/$(MODE)/mips/%.emulates: o/$(MODE)/% o/$(MODE)/mips/blink/blink o/third_party/qemu/qemu-mips $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-mips o/$(MODE)/mips/blink/blink $<
	@touch $@
o/$(MODE)/mipsel/%.emulates: o/$(MODE)/% o/$(MODE)/mipsel/blink/blink o/third_party/qemu/qemu-mipsel $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-mipsel o/$(MODE)/mipsel/blink/blink $<
	@touch $@
o/$(MODE)/mips64/%.emulates: o/$(MODE)/% o/$(MODE)/mips64/blink/blink o/third_party/qemu/qemu-mips64 $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-mips64 o/$(MODE)/mips64/blink/blink $<
	@touch $@
o/$(MODE)/mips64el/%.emulates: o/$(MODE)/% o/$(MODE)/mips64el/blink/blink o/third_party/qemu/qemu-mips64el $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-mips64el o/$(MODE)/mips64el/blink/blink $<
	@touch $@
o/$(MODE)/s390x/%.emulates: o/$(MODE)/% o/$(MODE)/s390x/blink/blink o/third_party/qemu/qemu-s390x $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-s390x o/$(MODE)/s390x/blink/blink $<
	@touch $@
o/$(MODE)/microblaze/%.emulates: o/$(MODE)/% o/$(MODE)/microblaze/blink/blink o/third_party/qemu/qemu-microblaze $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-microblaze o/$(MODE)/microblaze/blink/blink $<
	@touch $@
o/$(MODE)/powerpc/%.emulates: o/$(MODE)/% o/$(MODE)/powerpc/blink/blink o/third_party/qemu/qemu-powerpc $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-powerpc o/$(MODE)/powerpc/blink/blink $<
	@touch $@
o/$(MODE)/powerpc64le/%.emulates: o/$(MODE)/% o/$(MODE)/powerpc64le/blink/blink o/third_party/qemu/qemu-powerpc64le $(VM)
	@mkdir -p $(@D)
	$(VM) o/third_party/qemu/qemu-powerpc64le o/$(MODE)/powerpc64le/blink/blink $<
	@touch $@

o/$(MODE)/test:									\
	o/$(MODE)/test/blink

o/$(MODE)/test/emulates:							\
	o/$(MODE)/test/blink/emulates
