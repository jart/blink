#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

TAGS ?= /usr/bin/ctags

CFLAGS +=				\
	-g				\
	-O2				\
	-Wall				\
	-pthread			\
	-fno-ident			\
	-fno-common			\
	-fstrict-aliasing		\
	-fstrict-overflow		\
	-Wno-unused-function

CPPFLAGS +=				\
	-iquote.			\
	-D_POSIX_C_SOURCE=200809L	\
	-D_XOPEN_SOURCE=700		\
	-D_DARWIN_C_SOURCE		\
	-D_DEFAULT_SOURCE		\
	-D_GNU_SOURCE

LDLIBS +=				\
	-lm				\
	-pthread

TAGSFLAGS =								\
	-e								\
	-a								\
	--if0=no							\
	--langmap=c:.c.h.i						\
	--line-directives=yes

ifeq ($(MODE), tiny)
CPPFLAGS += -DNDEBUG
CFLAGS += -Os -fno-align-functions -fno-align-jumps -fno-align-labels -fno-align-loops
endif

ifeq ($(MODE), rel)
CPPFLAGS += -DNDEBUG
CFLAGS += -O2
endif

ifeq ($(MODE), opt)
CPPFLAGS += -DNDEBUG
CFLAGS += -O3
TARGET_ARCH = -march=native
endif

ifeq ($(MODE), asan)
CPPFLAGS += -fsanitize=address
LDLIBS += -fsanitize=address
endif

ifeq ($(MODE), ubsan)
CPPFLAGS += -fsanitize=undefined
LDLIBS += -fsanitize=undefined
endif

ifeq ($(MODE), tsan)
CC = clang++
AR = llvm-ar
CFLAGS += -xc++ -Werror -Wno-unused-parameter -Wno-missing-field-initializers
LDFLAGS += -fuse-ld=lld
CFLAGS += -fsanitize=thread
LDLIBS += -fsanitize=thread
endif

# TODO(jart): is this obsolete?
ifeq ($(MODE), aarch64)
CPPFLAGS += -DNDEBUG
CFLAGS += -Os
CC = o/third_party/gcc/aarch64/bin/aarch64-linux-musl-gcc
endif
