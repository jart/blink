#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi ──────────────────────┘

PKGS += TEST_BLINK
TEST_BLINK_FILES := $(wildcard test/blink/*)
TEST_BLINK_SRCS = $(filter %.c,$(TEST_BLINK_FILES))
TEST_BLINK_HDRS = $(filter %.h,$(TEST_BLINK_FILES))
TEST_BLINK_OBJS = $(TEST_BLINK_SRCS:%.c=o/$(MODE)/%.o)

# this file takes a minute to compile on raspi using -O2
o/$(MODE)/test/blink/x86_test.o				\
o/$(MODE)/i486/test/blink/x86_test.o			\
o/$(MODE)/m68k/test/blink/x86_test.o			\
o/$(MODE)/x86_64/test/blink/x86_test.o			\
o/$(MODE)/x86_64-gcc49/test/blink/x86_test.o		\
o/$(MODE)/arm/test/blink/x86_test.o			\
o/$(MODE)/aarch64/test/blink/x86_test.o			\
o/$(MODE)/riscv64/test/blink/x86_test.o			\
o/$(MODE)/mips/test/blink/x86_test.o			\
o/$(MODE)/mipsel/test/blink/x86_test.o			\
o/$(MODE)/mips64/test/blink/x86_test.o			\
o/$(MODE)/mips64el/test/blink/x86_test.o		\
o/$(MODE)/s390x/test/blink/x86_test.o			\
o/$(MODE)/powerpc/test/blink/x86_test.o			\
o/$(MODE)/powerpc64le/test/blink/x86_test.o:		\
		private CFLAGS += -O0

o/$(MODE)/test/blink/divmul_test.com: o/$(MODE)/test/blink/divmul_test.o o/$(MODE)/blink/blink.a
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/i486/test/blink/divmul_test.com: o/$(MODE)/i486/test/blink/divmul_test.o o/$(MODE)/i486/blink/blink.a
	o/third_party/gcc/i486/bin/i486-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/m68k/test/blink/divmul_test.com: o/$(MODE)/m68k/test/blink/divmul_test.o o/$(MODE)/m68k/blink/blink.a
	o/third_party/gcc/m68k/bin/m68k-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/x86_64/test/blink/divmul_test.com: o/$(MODE)/x86_64/test/blink/divmul_test.o o/$(MODE)/x86_64/blink/blink.a
	o/third_party/gcc/x86_64/bin/x86_64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/x86_64-gcc49/test/blink/divmul_test.com: o/$(MODE)/x86_64-gcc49/test/blink/divmul_test.o o/$(MODE)/x86_64-gcc49/blink/blink.a
	o/third_party/gcc/x86_64-gcc49/bin/x86_64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/arm/test/blink/divmul_test.com: o/$(MODE)/arm/test/blink/divmul_test.o o/$(MODE)/arm/blink/blink.a
	o/third_party/gcc/arm/bin/arm-linux-musleabi-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/aarch64/test/blink/divmul_test.com: o/$(MODE)/aarch64/test/blink/divmul_test.o o/$(MODE)/aarch64/blink/blink.a
	o/third_party/gcc/aarch64/bin/aarch64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/riscv64/test/blink/divmul_test.com: o/$(MODE)/riscv64/test/blink/divmul_test.o o/$(MODE)/riscv64/blink/blink.a
	o/third_party/gcc/riscv64/bin/riscv64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mips/test/blink/divmul_test.com: o/$(MODE)/mips/test/blink/divmul_test.o o/$(MODE)/mips/blink/blink.a
	o/third_party/gcc/mips/bin/mips-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mipsel/test/blink/divmul_test.com: o/$(MODE)/mipsel/test/blink/divmul_test.o o/$(MODE)/mipsel/blink/blink.a
	o/third_party/gcc/mipsel/bin/mipsel-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mips64/test/blink/divmul_test.com: o/$(MODE)/mips64/test/blink/divmul_test.o o/$(MODE)/mips64/blink/blink.a
	o/third_party/gcc/mips64/bin/mips64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mips64el/test/blink/divmul_test.com: o/$(MODE)/mips64el/test/blink/divmul_test.o o/$(MODE)/mips64el/blink/blink.a
	o/third_party/gcc/mips64el/bin/mips64el-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/s390x/test/blink/divmul_test.com: o/$(MODE)/s390x/test/blink/divmul_test.o o/$(MODE)/s390x/blink/blink.a
	o/third_party/gcc/s390x/bin/s390x-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/powerpc/test/blink/divmul_test.com: o/$(MODE)/powerpc/test/blink/divmul_test.o o/$(MODE)/powerpc/blink/blink.a
	o/third_party/gcc/powerpc/bin/powerpc-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/powerpc64le/test/blink/divmul_test.com: o/$(MODE)/powerpc64le/test/blink/divmul_test.o o/$(MODE)/powerpc64le/blink/blink.a
	o/third_party/gcc/powerpc64le/bin/powerpc64le-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@

o/$(MODE)/test/blink/modrm_test.com: o/$(MODE)/test/blink/modrm_test.o o/$(MODE)/blink/blink.a
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/i486/test/blink/modrm_test.com: o/$(MODE)/i486/test/blink/modrm_test.o o/$(MODE)/i486/blink/blink.a
	o/third_party/gcc/i486/bin/i486-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/m68k/test/blink/modrm_test.com: o/$(MODE)/m68k/test/blink/modrm_test.o o/$(MODE)/m68k/blink/blink.a
	o/third_party/gcc/m68k/bin/m68k-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/x86_64/test/blink/modrm_test.com: o/$(MODE)/x86_64/test/blink/modrm_test.o o/$(MODE)/x86_64/blink/blink.a
	o/third_party/gcc/x86_64/bin/x86_64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/x86_64-gcc49/test/blink/modrm_test.com: o/$(MODE)/x86_64-gcc49/test/blink/modrm_test.o o/$(MODE)/x86_64-gcc49/blink/blink.a
	o/third_party/gcc/x86_64-gcc49/bin/x86_64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/arm/test/blink/modrm_test.com: o/$(MODE)/arm/test/blink/modrm_test.o o/$(MODE)/arm/blink/blink.a
	o/third_party/gcc/arm/bin/arm-linux-musleabi-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/aarch64/test/blink/modrm_test.com: o/$(MODE)/aarch64/test/blink/modrm_test.o o/$(MODE)/aarch64/blink/blink.a
	o/third_party/gcc/aarch64/bin/aarch64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/riscv64/test/blink/modrm_test.com: o/$(MODE)/riscv64/test/blink/modrm_test.o o/$(MODE)/riscv64/blink/blink.a
	o/third_party/gcc/riscv64/bin/riscv64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mips/test/blink/modrm_test.com: o/$(MODE)/mips/test/blink/modrm_test.o o/$(MODE)/mips/blink/blink.a
	o/third_party/gcc/mips/bin/mips-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mipsel/test/blink/modrm_test.com: o/$(MODE)/mipsel/test/blink/modrm_test.o o/$(MODE)/mipsel/blink/blink.a
	o/third_party/gcc/mipsel/bin/mipsel-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mips64/test/blink/modrm_test.com: o/$(MODE)/mips64/test/blink/modrm_test.o o/$(MODE)/mips64/blink/blink.a
	o/third_party/gcc/mips64/bin/mips64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mips64el/test/blink/modrm_test.com: o/$(MODE)/mips64el/test/blink/modrm_test.o o/$(MODE)/mips64el/blink/blink.a
	o/third_party/gcc/mips64el/bin/mips64el-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/s390x/test/blink/modrm_test.com: o/$(MODE)/s390x/test/blink/modrm_test.o o/$(MODE)/s390x/blink/blink.a
	o/third_party/gcc/s390x/bin/s390x-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/powerpc/test/blink/modrm_test.com: o/$(MODE)/powerpc/test/blink/modrm_test.o o/$(MODE)/powerpc/blink/blink.a
	o/third_party/gcc/powerpc/bin/powerpc-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/powerpc64le/test/blink/modrm_test.com: o/$(MODE)/powerpc64le/test/blink/modrm_test.o o/$(MODE)/powerpc64le/blink/blink.a
	o/third_party/gcc/powerpc64le/bin/powerpc64le-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@

o/$(MODE)/test/blink/x86_test.com: o/$(MODE)/test/blink/x86_test.o o/$(MODE)/blink/blink.a
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/i486/test/blink/x86_test.com: o/$(MODE)/i486/test/blink/x86_test.o o/$(MODE)/i486/blink/blink.a
	o/third_party/gcc/i486/bin/i486-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/m68k/test/blink/x86_test.com: o/$(MODE)/m68k/test/blink/x86_test.o o/$(MODE)/m68k/blink/blink.a
	o/third_party/gcc/m68k/bin/m68k-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/x86_64/test/blink/x86_test.com: o/$(MODE)/x86_64/test/blink/x86_test.o o/$(MODE)/x86_64/blink/blink.a
	o/third_party/gcc/x86_64/bin/x86_64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/x86_64-gcc49/test/blink/x86_test.com: o/$(MODE)/x86_64-gcc49/test/blink/x86_test.o o/$(MODE)/x86_64-gcc49/blink/blink.a
	o/third_party/gcc/x86_64-gcc49/bin/x86_64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/arm/test/blink/x86_test.com: o/$(MODE)/arm/test/blink/x86_test.o o/$(MODE)/arm/blink/blink.a
	o/third_party/gcc/arm/bin/arm-linux-musleabi-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/aarch64/test/blink/x86_test.com: o/$(MODE)/aarch64/test/blink/x86_test.o o/$(MODE)/aarch64/blink/blink.a
	o/third_party/gcc/aarch64/bin/aarch64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/riscv64/test/blink/x86_test.com: o/$(MODE)/riscv64/test/blink/x86_test.o o/$(MODE)/riscv64/blink/blink.a
	o/third_party/gcc/riscv64/bin/riscv64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mips/test/blink/x86_test.com: o/$(MODE)/mips/test/blink/x86_test.o o/$(MODE)/mips/blink/blink.a
	o/third_party/gcc/mips/bin/mips-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mipsel/test/blink/x86_test.com: o/$(MODE)/mipsel/test/blink/x86_test.o o/$(MODE)/mipsel/blink/blink.a
	o/third_party/gcc/mipsel/bin/mipsel-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mips64/test/blink/x86_test.com: o/$(MODE)/mips64/test/blink/x86_test.o o/$(MODE)/mips64/blink/blink.a
	o/third_party/gcc/mips64/bin/mips64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mips64el/test/blink/x86_test.com: o/$(MODE)/mips64el/test/blink/x86_test.o o/$(MODE)/mips64el/blink/blink.a
	o/third_party/gcc/mips64el/bin/mips64el-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/s390x/test/blink/x86_test.com: o/$(MODE)/s390x/test/blink/x86_test.o o/$(MODE)/s390x/blink/blink.a
	o/third_party/gcc/s390x/bin/s390x-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/powerpc/test/blink/x86_test.com: o/$(MODE)/powerpc/test/blink/x86_test.o o/$(MODE)/powerpc/blink/blink.a
	o/third_party/gcc/powerpc/bin/powerpc-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/powerpc64le/test/blink/x86_test.com: o/$(MODE)/powerpc64le/test/blink/x86_test.o o/$(MODE)/powerpc64le/blink/blink.a
	o/third_party/gcc/powerpc64le/bin/powerpc64le-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@

o/$(MODE)/test/blink/ldbl_test.com: o/$(MODE)/test/blink/ldbl_test.o o/$(MODE)/blink/blink.a
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/i486/test/blink/ldbl_test.com: o/$(MODE)/i486/test/blink/ldbl_test.o o/$(MODE)/i486/blink/blink.a
	o/third_party/gcc/i486/bin/i486-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/m68k/test/blink/ldbl_test.com: o/$(MODE)/m68k/test/blink/ldbl_test.o o/$(MODE)/m68k/blink/blink.a
	o/third_party/gcc/m68k/bin/m68k-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/x86_64/test/blink/ldbl_test.com: o/$(MODE)/x86_64/test/blink/ldbl_test.o o/$(MODE)/x86_64/blink/blink.a
	o/third_party/gcc/x86_64/bin/x86_64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/x86_64-gcc49/test/blink/ldbl_test.com: o/$(MODE)/x86_64-gcc49/test/blink/ldbl_test.o o/$(MODE)/x86_64-gcc49/blink/blink.a
	o/third_party/gcc/x86_64-gcc49/bin/x86_64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/arm/test/blink/ldbl_test.com: o/$(MODE)/arm/test/blink/ldbl_test.o o/$(MODE)/arm/blink/blink.a
	o/third_party/gcc/arm/bin/arm-linux-musleabi-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/aarch64/test/blink/ldbl_test.com: o/$(MODE)/aarch64/test/blink/ldbl_test.o o/$(MODE)/aarch64/blink/blink.a
	o/third_party/gcc/aarch64/bin/aarch64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/riscv64/test/blink/ldbl_test.com: o/$(MODE)/riscv64/test/blink/ldbl_test.o o/$(MODE)/riscv64/blink/blink.a
	o/third_party/gcc/riscv64/bin/riscv64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mips/test/blink/ldbl_test.com: o/$(MODE)/mips/test/blink/ldbl_test.o o/$(MODE)/mips/blink/blink.a
	o/third_party/gcc/mips/bin/mips-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mipsel/test/blink/ldbl_test.com: o/$(MODE)/mipsel/test/blink/ldbl_test.o o/$(MODE)/mipsel/blink/blink.a
	o/third_party/gcc/mipsel/bin/mipsel-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mips64/test/blink/ldbl_test.com: o/$(MODE)/mips64/test/blink/ldbl_test.o o/$(MODE)/mips64/blink/blink.a
	o/third_party/gcc/mips64/bin/mips64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mips64el/test/blink/ldbl_test.com: o/$(MODE)/mips64el/test/blink/ldbl_test.o o/$(MODE)/mips64el/blink/blink.a
	o/third_party/gcc/mips64el/bin/mips64el-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/s390x/test/blink/ldbl_test.com: o/$(MODE)/s390x/test/blink/ldbl_test.o o/$(MODE)/s390x/blink/blink.a
	o/third_party/gcc/s390x/bin/s390x-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/powerpc/test/blink/ldbl_test.com: o/$(MODE)/powerpc/test/blink/ldbl_test.o o/$(MODE)/powerpc/blink/blink.a
	o/third_party/gcc/powerpc/bin/powerpc-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/powerpc64le/test/blink/ldbl_test.com: o/$(MODE)/powerpc64le/test/blink/ldbl_test.o o/$(MODE)/powerpc64le/blink/blink.a
	o/third_party/gcc/powerpc64le/bin/powerpc64le-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@

o/$(MODE)/test/blink/disinst_test.com: o/$(MODE)/test/blink/disinst_test.o o/$(MODE)/blink/blink.a
	$(CC) $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/i486/test/blink/disinst_test.com: o/$(MODE)/i486/test/blink/disinst_test.o o/$(MODE)/i486/blink/blink.a
	o/third_party/gcc/i486/bin/i486-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/m68k/test/blink/disinst_test.com: o/$(MODE)/m68k/test/blink/disinst_test.o o/$(MODE)/m68k/blink/blink.a
	o/third_party/gcc/m68k/bin/m68k-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/x86_64/test/blink/disinst_test.com: o/$(MODE)/x86_64/test/blink/disinst_test.o o/$(MODE)/x86_64/blink/blink.a
	o/third_party/gcc/x86_64/bin/x86_64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/x86_64-gcc49/test/blink/disinst_test.com: o/$(MODE)/x86_64-gcc49/test/blink/disinst_test.o o/$(MODE)/x86_64-gcc49/blink/blink.a
	o/third_party/gcc/x86_64-gcc49/bin/x86_64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/arm/test/blink/disinst_test.com: o/$(MODE)/arm/test/blink/disinst_test.o o/$(MODE)/arm/blink/blink.a
	o/third_party/gcc/arm/bin/arm-linux-musleabi-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/aarch64/test/blink/disinst_test.com: o/$(MODE)/aarch64/test/blink/disinst_test.o o/$(MODE)/aarch64/blink/blink.a
	o/third_party/gcc/aarch64/bin/aarch64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/riscv64/test/blink/disinst_test.com: o/$(MODE)/riscv64/test/blink/disinst_test.o o/$(MODE)/riscv64/blink/blink.a
	o/third_party/gcc/riscv64/bin/riscv64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mips/test/blink/disinst_test.com: o/$(MODE)/mips/test/blink/disinst_test.o o/$(MODE)/mips/blink/blink.a
	o/third_party/gcc/mips/bin/mips-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mipsel/test/blink/disinst_test.com: o/$(MODE)/mipsel/test/blink/disinst_test.o o/$(MODE)/mipsel/blink/blink.a
	o/third_party/gcc/mipsel/bin/mipsel-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mips64/test/blink/disinst_test.com: o/$(MODE)/mips64/test/blink/disinst_test.o o/$(MODE)/mips64/blink/blink.a
	o/third_party/gcc/mips64/bin/mips64-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/mips64el/test/blink/disinst_test.com: o/$(MODE)/mips64el/test/blink/disinst_test.o o/$(MODE)/mips64el/blink/blink.a
	o/third_party/gcc/mips64el/bin/mips64el-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/s390x/test/blink/disinst_test.com: o/$(MODE)/s390x/test/blink/disinst_test.o o/$(MODE)/s390x/blink/blink.a
	o/third_party/gcc/s390x/bin/s390x-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/powerpc/test/blink/disinst_test.com: o/$(MODE)/powerpc/test/blink/disinst_test.o o/$(MODE)/powerpc/blink/blink.a
	o/third_party/gcc/powerpc/bin/powerpc-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@
o/$(MODE)/powerpc64le/test/blink/disinst_test.com: o/$(MODE)/powerpc64le/test/blink/disinst_test.o o/$(MODE)/powerpc64le/blink/blink.a
	o/third_party/gcc/powerpc64le/bin/powerpc64le-linux-musl-gcc -static $(LDFLAGS) $(TARGET_ARCH) $^ $(LOADLIBES) $(LDLIBS) -o $@

o/$(MODE)/test/blink:							\
		$(TEST_BLINK_OBJS)					\
		o/$(MODE)/test/blink/divmul_test.com.runs		\
		o/$(MODE)/test/blink/modrm_test.com.runs		\
		o/$(MODE)/test/blink/x86_test.com.runs			\
		o/$(MODE)/test/blink/ldbl_test.com.runs			\
		o/$(MODE)/test/blink/disinst_test.com.runs

o/$(MODE)/test/blink/emulates:						\
		o/$(MODE)/blink/blink					\
		o/$(MODE)/i486/blink/blink				\
		o/$(MODE)/m68k/blink/blink				\
		o/$(MODE)/x86_64/blink/blink				\
		o/$(MODE)/arm/blink/blink				\
		o/$(MODE)/aarch64/blink/blink				\
		o/$(MODE)/riscv64/blink/blink				\
		o/$(MODE)/mips/blink/blink				\
		o/$(MODE)/mipsel/blink/blink				\
		o/$(MODE)/mips64/blink/blink				\
		o/$(MODE)/mips64el/blink/blink				\
		o/$(MODE)/s390x/blink/blink				\
		o/$(MODE)/powerpc/blink/blink				\
		o/$(MODE)/powerpc64le/blink/blink			\
		o/$(MODE)/blink/blink					\
		o/$(MODE)/i486/blink/blink				\
		o/$(MODE)/m68k/blink/blink				\
		o/$(MODE)/x86_64/blink/blink				\
		o/$(MODE)/arm/blink/blink				\
		o/$(MODE)/aarch64/blink/blink				\
		o/$(MODE)/riscv64/blink/blink				\
		o/$(MODE)/mips/blink/blink				\
		o/$(MODE)/mipsel/blink/blink				\
		o/$(MODE)/mips64/blink/blink				\
		o/$(MODE)/mips64el/blink/blink				\
		o/$(MODE)/s390x/blink/blink				\
		o/$(MODE)/powerpc/blink/blink				\
		o/$(MODE)/powerpc64le/blink/blink			\
		o/$(MODE)/i486/test/blink/divmul_test.com.runs		\
		o/$(MODE)/m68k/test/blink/divmul_test.com.runs		\
		o/$(MODE)/x86_64/test/blink/divmul_test.com.runs	\
		o/$(MODE)/arm/test/blink/divmul_test.com.runs		\
		o/$(MODE)/aarch64/test/blink/divmul_test.com.runs	\
		o/$(MODE)/riscv64/test/blink/divmul_test.com.runs	\
		o/$(MODE)/mips/test/blink/divmul_test.com.runs		\
		o/$(MODE)/mipsel/test/blink/divmul_test.com.runs	\
		o/$(MODE)/mips64/test/blink/divmul_test.com.runs	\
		o/$(MODE)/mips64el/test/blink/divmul_test.com.runs	\
		o/$(MODE)/s390x/test/blink/divmul_test.com.runs		\
		o/$(MODE)/powerpc/test/blink/divmul_test.com.runs	\
		o/$(MODE)/powerpc64le/test/blink/divmul_test.com.runs	\
		o/$(MODE)/i486/test/blink/modrm_test.com.runs		\
		o/$(MODE)/m68k/test/blink/modrm_test.com.runs		\
		o/$(MODE)/x86_64/test/blink/modrm_test.com.runs		\
		o/$(MODE)/arm/test/blink/modrm_test.com.runs		\
		o/$(MODE)/aarch64/test/blink/modrm_test.com.runs	\
		o/$(MODE)/riscv64/test/blink/modrm_test.com.runs	\
		o/$(MODE)/mips/test/blink/modrm_test.com.runs		\
		o/$(MODE)/mipsel/test/blink/modrm_test.com.runs		\
		o/$(MODE)/mips64/test/blink/modrm_test.com.runs		\
		o/$(MODE)/mips64el/test/blink/modrm_test.com.runs	\
		o/$(MODE)/s390x/test/blink/modrm_test.com.runs		\
		o/$(MODE)/powerpc/test/blink/modrm_test.com.runs	\
		o/$(MODE)/powerpc64le/test/blink/modrm_test.com.runs	\
		o/$(MODE)/i486/test/blink/x86_test.com.runs		\
		o/$(MODE)/m68k/test/blink/x86_test.com.runs		\
		o/$(MODE)/x86_64/test/blink/x86_test.com.runs		\
		o/$(MODE)/arm/test/blink/x86_test.com.runs		\
		o/$(MODE)/aarch64/test/blink/x86_test.com.runs		\
		o/$(MODE)/riscv64/test/blink/x86_test.com.runs		\
		o/$(MODE)/mips/test/blink/x86_test.com.runs		\
		o/$(MODE)/mipsel/test/blink/x86_test.com.runs		\
		o/$(MODE)/mips64/test/blink/x86_test.com.runs		\
		o/$(MODE)/mips64el/test/blink/x86_test.com.runs		\
		o/$(MODE)/s390x/test/blink/x86_test.com.runs		\
		o/$(MODE)/powerpc/test/blink/x86_test.com.runs		\
		o/$(MODE)/powerpc64le/test/blink/x86_test.com.runs	\
		o/$(MODE)/i486/test/blink/ldbl_test.com.runs		\
		o/$(MODE)/m68k/test/blink/ldbl_test.com.runs		\
		o/$(MODE)/x86_64/test/blink/ldbl_test.com.runs		\
		o/$(MODE)/arm/test/blink/ldbl_test.com.runs		\
		o/$(MODE)/aarch64/test/blink/ldbl_test.com.runs		\
		o/$(MODE)/riscv64/test/blink/ldbl_test.com.runs		\
		o/$(MODE)/mips/test/blink/ldbl_test.com.runs		\
		o/$(MODE)/mipsel/test/blink/ldbl_test.com.runs		\
		o/$(MODE)/mips64/test/blink/ldbl_test.com.runs		\
		o/$(MODE)/mips64el/test/blink/ldbl_test.com.runs	\
		o/$(MODE)/s390x/test/blink/ldbl_test.com.runs		\
		o/$(MODE)/powerpc/test/blink/ldbl_test.com.runs		\
		o/$(MODE)/powerpc64le/test/blink/ldbl_test.com.runs	\
		o/$(MODE)/i486/test/blink/disinst_test.com.runs		\
		o/$(MODE)/m68k/test/blink/disinst_test.com.runs		\
		o/$(MODE)/x86_64/test/blink/disinst_test.com.runs	\
		o/$(MODE)/arm/test/blink/disinst_test.com.runs		\
		o/$(MODE)/aarch64/test/blink/disinst_test.com.runs	\
		o/$(MODE)/riscv64/test/blink/disinst_test.com.runs	\
		o/$(MODE)/mips/test/blink/disinst_test.com.runs		\
		o/$(MODE)/mipsel/test/blink/disinst_test.com.runs	\
		o/$(MODE)/mips64/test/blink/disinst_test.com.runs	\
		o/$(MODE)/mips64el/test/blink/disinst_test.com.runs	\
		o/$(MODE)/s390x/test/blink/disinst_test.com.runs	\
		o/$(MODE)/powerpc/test/blink/disinst_test.com.runs	\
		o/$(MODE)/powerpc64le/test/blink/disinst_test.com.runs
