#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

PKGS += BLINK
BLINK_FILES := $(wildcard blink/*)
BLINK_SRCS = $(filter %.c,$(BLINK_FILES))
BLINK_HDRS = $(filter %.h,$(BLINK_FILES))
BLINK_SRCS = $(filter %.c,$(BLINK_FILES))
BLINK_OBJS = $(BLINK_SRCS:%.c=o/$(MODE)/%.o)

BLINK_NEED_BUILDINFO_SRCS =				\
	blink/blink.c					\
	blink/syscall.c					\
	blink/blinkenlights.c

BLINK_NEED_BUILDINFO_OBJS =				\
	$(BLINK_NEED_BUILDINFO_SRCS:%.c=o/$(MODE)/%.o)	\
	$(foreach ARCH,$(ARCHITECTURES),$(foreach SRC,$(BLINK_NEED_BUILDINFO_SRCS),$(SRC:%.c=o/$(MODE)/$(ARCH)/%.o)))

$(BLINK_NEED_BUILDINFO_OBJS): private CPPFLAGS += $(CONFIG_CPPFLAGS)

# micro-ops need to be compiled with the greatest of care
o/$(MODE)/blink/uop.o: private CFLAGS += $(UOPFLAGS)

# Actually Portable Executable
# make m=cosmo o/cosmo/blink/blink.com
# needs cosmopolitan/tool/scripts/cosmocc
o/cosmo/blink/blink.com: o/$(MODE)/blink/blink
	objcopy -S -O binary $< $@
o/cosmo/blink/blinkenlights.com: o/$(MODE)/blink/blinkenlights
	objcopy -S -O binary $< $@

# vectorization makes code smaller
o/$(MODE)/blink/sse2.o: private CFLAGS += -O3
o/$(MODE)/x86_64/blink/sse2.o: private CFLAGS += -O3
o/$(MODE)/x86_64-gcc49/blink/sse2.o: private CFLAGS += -O3
o/$(MODE)/aarch64/blink/sse2.o: private CFLAGS += -O3

# these files have big switch statements
o/tiny/x86_64/blink/cvt.o: private CFLAGS += -fpie
o/tiny/x86_64-gcc49/blink/cvt.o: private CFLAGS += -fpie
o/tiny/aarch64/blink/cvt.o: private CFLAGS += -fpie
o/tiny/x86_64/blink/xlat.o: private CFLAGS += -fpie
o/tiny/x86_64-gcc49/blink/xlat.o: private CFLAGS += -fpie
o/tiny/aarch64/blink/xlat.o: private CFLAGS += -fpie
o/tiny/x86_64/blink/fpu.o: private CFLAGS += -fpie
o/tiny/x86_64-gcc49/blink/fpu.o: private CFLAGS += -fpie
o/tiny/aarch64/blink/fpu.o: private CFLAGS += -fpie
o/tiny/x86_64/blink/flags.o: private CFLAGS += -fpie
o/tiny/x86_64-gcc49/blink/flags.o: private CFLAGS += -fpie
o/tiny/aarch64/blink/flags.o: private CFLAGS += -fpie
o/tiny/x86_64/blink/x86.o: private CFLAGS += -fno-jump-tables
o/tiny/x86_64-gcc49/blink/x86.o: private CFLAGS += -fno-jump-tables
o/tiny/aarch64/blink/x86.o: private CFLAGS += -fno-jump-tables
o/tiny/x86_64/blink/uop.o: private CFLAGS += -fno-jump-tables
o/tiny/x86_64-gcc49/blink/uop.o: private CFLAGS += -fno-jump-tables
o/tiny/aarch64/blink/uop.o: private CFLAGS += -fno-jump-tables
o/tiny/x86_64/blink/syscall.o: private CFLAGS += -fpie
o/tiny/x86_64-gcc49/blink/syscall.o: private CFLAGS += -fpie
o/tiny/aarch64/blink/syscall.o: private CFLAGS += -fpie

o/$(MODE)/blink/blink.a: $(filter-out %/blink.o,$(filter-out %/blinkenlights.o,$(BLINK_OBJS)))
o/$(MODE)/i486/blink/blink.a: $(filter-out %/blink.o,$(filter-out %/blinkenlights.o,$(BLINK_SRCS:%.c=o/$(MODE)/i486/%.o)))
o/$(MODE)/m68k/blink/blink.a: $(filter-out %/blink.o,$(filter-out %/blinkenlights.o,$(BLINK_SRCS:%.c=o/$(MODE)/m68k/%.o)))
o/$(MODE)/x86_64/blink/blink.a: $(filter-out %/blink.o,$(filter-out %/blinkenlights.o,$(BLINK_SRCS:%.c=o/$(MODE)/x86_64/%.o)))
o/$(MODE)/x86_64-gcc49/blink/blink.a: $(filter-out %/blink.o,$(filter-out %/blinkenlights.o,$(BLINK_SRCS:%.c=o/$(MODE)/x86_64-gcc49/%.o)))
o/$(MODE)/arm/blink/blink.a: $(filter-out %/blink.o,$(filter-out %/blinkenlights.o,$(BLINK_SRCS:%.c=o/$(MODE)/arm/%.o)))
o/$(MODE)/aarch64/blink/blink.a: $(filter-out %/blink.o,$(filter-out %/blinkenlights.o,$(BLINK_SRCS:%.c=o/$(MODE)/aarch64/%.o)))
o/$(MODE)/riscv64/blink/blink.a: $(filter-out %/blink.o,$(filter-out %/blinkenlights.o,$(BLINK_SRCS:%.c=o/$(MODE)/riscv64/%.o)))
o/$(MODE)/mips/blink/blink.a: $(filter-out %/blink.o,$(filter-out %/blinkenlights.o,$(BLINK_SRCS:%.c=o/$(MODE)/mips/%.o)))
o/$(MODE)/mipsel/blink/blink.a: $(filter-out %/blink.o,$(filter-out %/blinkenlights.o,$(BLINK_SRCS:%.c=o/$(MODE)/mipsel/%.o)))
o/$(MODE)/mips64/blink/blink.a: $(filter-out %/blink.o,$(filter-out %/blinkenlights.o,$(BLINK_SRCS:%.c=o/$(MODE)/mips64/%.o)))
o/$(MODE)/mips64el/blink/blink.a: $(filter-out %/blink.o,$(filter-out %/blinkenlights.o,$(BLINK_SRCS:%.c=o/$(MODE)/mips64el/%.o)))
o/$(MODE)/s390x/blink/blink.a: $(filter-out %/blink.o,$(filter-out %/blinkenlights.o,$(BLINK_SRCS:%.c=o/$(MODE)/s390x/%.o)))
o/$(MODE)/microblaze/blink/blink.a: $(filter-out %/blink.o,$(filter-out %/blinkenlights.o,$(BLINK_SRCS:%.c=o/$(MODE)/microblaze/%.o)))
o/$(MODE)/powerpc/blink/blink.a: $(filter-out %/blink.o,$(filter-out %/blinkenlights.o,$(BLINK_SRCS:%.c=o/$(MODE)/powerpc/%.o)))
o/$(MODE)/powerpc64le/blink/blink.a: $(filter-out %/blink.o,$(filter-out %/blinkenlights.o,$(BLINK_SRCS:%.c=o/$(MODE)/powerpc64le/%.o)))

o/$(MODE)/blink/blink: o/$(MODE)/blink/blink.o o/$(MODE)/blink/blink.a
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/i486/blink/blink: o/$(MODE)/i486/blink/blink.o o/$(MODE)/i486/blink/blink.a
	$(VM) o/third_party/gcc/i486/bin/i486-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/m68k/blink/blink: o/$(MODE)/m68k/blink/blink.o o/$(MODE)/m68k/blink/blink.a
	$(VM) o/third_party/gcc/m68k/bin/m68k-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/x86_64/blink/blink: o/$(MODE)/x86_64/blink/blink.o o/$(MODE)/x86_64/blink/blink.a
	$(VM) o/third_party/gcc/x86_64/bin/x86_64-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/x86_64-gcc49/blink/blink: o/$(MODE)/x86_64-gcc49/blink/blink.o o/$(MODE)/x86_64-gcc49/blink/blink.a
	$(VM) o/third_party/gcc/x86_64-gcc49/bin/x86_64-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/arm/blink/blink: o/$(MODE)/arm/blink/blink.o o/$(MODE)/arm/blink/blink.a
	$(VM) o/third_party/gcc/arm/bin/arm-linux-musleabi-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/aarch64/blink/blink: o/$(MODE)/aarch64/blink/blink.o o/$(MODE)/aarch64/blink/blink.a
	$(VM) o/third_party/gcc/aarch64/bin/aarch64-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/riscv64/blink/blink: o/$(MODE)/riscv64/blink/blink.o o/$(MODE)/riscv64/blink/blink.a
	$(VM) o/third_party/gcc/riscv64/bin/riscv64-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/mips/blink/blink: o/$(MODE)/mips/blink/blink.o o/$(MODE)/mips/blink/blink.a
	$(VM) o/third_party/gcc/mips/bin/mips-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/mipsel/blink/blink: o/$(MODE)/mipsel/blink/blink.o o/$(MODE)/mipsel/blink/blink.a
	$(VM) o/third_party/gcc/mipsel/bin/mipsel-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/mips64/blink/blink: o/$(MODE)/mips64/blink/blink.o o/$(MODE)/mips64/blink/blink.a
	$(VM) o/third_party/gcc/mips64/bin/mips64-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/mips64el/blink/blink: o/$(MODE)/mips64el/blink/blink.o o/$(MODE)/mips64el/blink/blink.a
	$(VM) o/third_party/gcc/mips64el/bin/mips64el-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/s390x/blink/blink: o/$(MODE)/s390x/blink/blink.o o/$(MODE)/s390x/blink/blink.a
	$(VM) o/third_party/gcc/s390x/bin/s390x-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/microblaze/blink/blink: o/$(MODE)/microblaze/blink/blink.o o/$(MODE)/microblaze/blink/blink.a
	$(VM) o/third_party/gcc/microblaze/bin/microblaze-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/powerpc/blink/blink: o/$(MODE)/powerpc/blink/blink.o o/$(MODE)/powerpc/blink/blink.a
	$(VM) o/third_party/gcc/powerpc/bin/powerpc-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/powerpc64le/blink/blink: o/$(MODE)/powerpc64le/blink/blink.o o/$(MODE)/powerpc64le/blink/blink.a
	$(VM) o/third_party/gcc/powerpc64le/bin/powerpc64le-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@

o/$(MODE)/blink/blinkenlights.html: o/$(MODE)/blink/blinkenlights.o o/$(MODE)/blink/blink.a $(ZLIB)
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/blink/blinkenlights: o/$(MODE)/blink/blinkenlights.o o/$(MODE)/blink/blink.a $(ZLIB)
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/i486/blink/blinkenlights: o/$(MODE)/i486/blink/blinkenlights.o o/$(MODE)/i486/blink/blink.a o/$(MODE)/i486/third_party/libz/zlib.a
	$(VM) o/third_party/gcc/i486/bin/i486-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/m68k/blink/blinkenlights: o/$(MODE)/m68k/blink/blinkenlights.o o/$(MODE)/m68k/blink/blink.a o/$(MODE)/m68k/third_party/libz/zlib.a
	$(VM) o/third_party/gcc/m68k/bin/m68k-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/x86_64/blink/blinkenlights: o/$(MODE)/x86_64/blink/blinkenlights.o o/$(MODE)/x86_64/blink/blink.a o/$(MODE)/x86_64/third_party/libz/zlib.a
	$(VM) o/third_party/gcc/x86_64/bin/x86_64-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/x86_64-gcc49/blink/blinkenlights: o/$(MODE)/x86_64-gcc49/blink/blinkenlights.o o/$(MODE)/x86_64-gcc49/blink/blink.a o/$(MODE)/x86_64-gcc49/third_party/libz/zlib.a
	$(VM) o/third_party/gcc/x86_64-gcc49/bin/x86_64-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/arm/blink/blinkenlights: o/$(MODE)/arm/blink/blinkenlights.o o/$(MODE)/arm/blink/blink.a o/$(MODE)/arm/third_party/libz/zlib.a
	$(VM) o/third_party/gcc/arm/bin/arm-linux-musleabi-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/aarch64/blink/blinkenlights: o/$(MODE)/aarch64/blink/blinkenlights.o o/$(MODE)/aarch64/blink/blink.a o/$(MODE)/aarch64/third_party/libz/zlib.a
	$(VM) o/third_party/gcc/aarch64/bin/aarch64-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/riscv64/blink/blinkenlights: o/$(MODE)/riscv64/blink/blinkenlights.o o/$(MODE)/riscv64/blink/blink.a o/$(MODE)/riscv64/third_party/libz/zlib.a
	$(VM) o/third_party/gcc/riscv64/bin/riscv64-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/mips/blink/blinkenlights: o/$(MODE)/mips/blink/blinkenlights.o o/$(MODE)/mips/blink/blink.a o/$(MODE)/mips/third_party/libz/zlib.a
	$(VM) o/third_party/gcc/mips/bin/mips-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/mipsel/blink/blinkenlights: o/$(MODE)/mipsel/blink/blinkenlights.o o/$(MODE)/mipsel/blink/blink.a o/$(MODE)/mipsel/third_party/libz/zlib.a
	$(VM) o/third_party/gcc/mipsel/bin/mipsel-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/mips64/blink/blinkenlights: o/$(MODE)/mips64/blink/blinkenlights.o o/$(MODE)/mips64/blink/blink.a o/$(MODE)/mips64/third_party/libz/zlib.a
	$(VM) o/third_party/gcc/mips64/bin/mips64-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/mips64el/blink/blinkenlights: o/$(MODE)/mips64el/blink/blinkenlights.o o/$(MODE)/mips64el/blink/blink.a o/$(MODE)/mips64el/third_party/libz/zlib.a
	$(VM) o/third_party/gcc/mips64el/bin/mips64el-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/s390x/blink/blinkenlights: o/$(MODE)/s390x/blink/blinkenlights.o o/$(MODE)/s390x/blink/blink.a o/$(MODE)/s390x/third_party/libz/zlib.a
	$(VM) o/third_party/gcc/s390x/bin/s390x-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/microblaze/blink/blinkenlights: o/$(MODE)/microblaze/blink/blinkenlights.o o/$(MODE)/microblaze/blink/blink.a o/$(MODE)/microblaze/third_party/libz/zlib.a
	$(VM) o/third_party/gcc/microblaze/bin/microblaze-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/powerpc/blink/blinkenlights: o/$(MODE)/powerpc/blink/blinkenlights.o o/$(MODE)/powerpc/blink/blink.a o/$(MODE)/powerpc/third_party/libz/zlib.a
	$(VM) o/third_party/gcc/powerpc/bin/powerpc-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@
o/$(MODE)/powerpc64le/blink/blinkenlights: o/$(MODE)/powerpc64le/blink/blinkenlights.o o/$(MODE)/powerpc64le/blink/blink.a o/$(MODE)/powerpc64le/third_party/libz/zlib.a
	$(VM) o/third_party/gcc/powerpc64le/bin/powerpc64le-linux-musl-gcc $(LDFLAGS_STATIC) $^ -o $@

o/$(MODE)/blink/oneoff.com: o/$(MODE)/blink/oneoff.o o/$(MODE)/blink/blink.a
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@

o/$(MODE)/blink:				\
		o/$(MODE)/blink/blinkenlights	\
		o/$(MODE)/blink/blink		\
		$(BLINK_HDRS:%=o/$(MODE)/%.ok)
