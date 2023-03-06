#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

PKGS += ZLIB
CFLAGS += -isystem third_party/libz

ZLIB = o/$(MODE)/third_party/libz/zlib.a
ZLIB_FILES := $(wildcard third_party/libz/*)
ZLIB_SRCS = $(filter %.c,$(ZLIB_FILES))
ZLIB_HDRS = $(filter %.h,$(ZLIB_FILES))
ZLIB_OBJS = $(ZLIB_SRCS:%.c=o/$(MODE)/%.o)

$(ZLIB_OBJS): private CFLAGS += -xc -w

o/$(MODE)/third_party/libz/zlib.a: $(ZLIB_OBJS)
o/$(MODE)/i486/third_party/libz/zlib.a: $(ZLIB_SRCS:%.c=o/$(MODE)/i486/%.o)
o/$(MODE)/m68k/third_party/libz/zlib.a: $(ZLIB_SRCS:%.c=o/$(MODE)/m68k/%.o)
o/$(MODE)/x86_64/third_party/libz/zlib.a: $(ZLIB_SRCS:%.c=o/$(MODE)/x86_64/%.o)
o/$(MODE)/x86_64-gcc49/third_party/libz/zlib.a: $(ZLIB_SRCS:%.c=o/$(MODE)/x86_64-gcc49/%.o)
o/$(MODE)/arm/third_party/libz/zlib.a: $(ZLIB_SRCS:%.c=o/$(MODE)/arm/%.o)
o/$(MODE)/aarch64/third_party/libz/zlib.a: $(ZLIB_SRCS:%.c=o/$(MODE)/aarch64/%.o)
o/$(MODE)/riscv64/third_party/libz/zlib.a: $(ZLIB_SRCS:%.c=o/$(MODE)/riscv64/%.o)
o/$(MODE)/mips/third_party/libz/zlib.a: $(ZLIB_SRCS:%.c=o/$(MODE)/mips/%.o)
o/$(MODE)/mipsel/third_party/libz/zlib.a: $(ZLIB_SRCS:%.c=o/$(MODE)/mipsel/%.o)
o/$(MODE)/mips64/third_party/libz/zlib.a: $(ZLIB_SRCS:%.c=o/$(MODE)/mips64/%.o)
o/$(MODE)/mips64el/third_party/libz/zlib.a: $(ZLIB_SRCS:%.c=o/$(MODE)/mips64el/%.o)
o/$(MODE)/s390x/third_party/libz/zlib.a: $(ZLIB_SRCS:%.c=o/$(MODE)/s390x/%.o)
o/$(MODE)/microblaze/third_party/libz/zlib.a: $(ZLIB_SRCS:%.c=o/$(MODE)/microblaze/%.o)
o/$(MODE)/powerpc/third_party/libz/zlib.a: $(ZLIB_SRCS:%.c=o/$(MODE)/powerpc/%.o)
o/$(MODE)/powerpc64le/third_party/libz/zlib.a: $(ZLIB_SRCS:%.c=o/$(MODE)/powerpc64le/%.o)

o/$(MODE)/third_party/libz:	\
	o/$(MODE)/third_party/libz/zlib.a
