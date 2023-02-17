#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

TAGS ?= /usr/bin/ctags

IMAGE_BASE_VIRTUAL = 0x23000000

ifeq ($(HOST_SYSTEM), Haiku)
CFLAGS += -fpic
endif

CPPFLAGS += -iquote.

ifeq ($(HOST_SYSTEM), Haiku)
LDLIBS += -lroot -lnetwork -lbsd
endif

ifneq ($(HOST_SYSTEM), OpenBSD)
CFLAGS += -gdwarf-2
endif

# FreeBSD loads executables to 0x200000 by default which is likely to
# overlap the static Linux guest binary, we usually load to 0x400000.
ifeq ($(HOST_SYSTEM), FreeBSD)
LDFLAGS += -Wl,--image-base=$(IMAGE_BASE_VIRTUAL)
endif

LDFLAGS_STATIC =			\
	-static				\
	-fno-exceptions			\
	-fno-unwind-tables		\
	-fno-asynchronous-unwind-tables	\
	-Wl,-z,max-page-size=4096	\
	-Wl,-z,common-page-size=4096	\
	-Wl,-z,norelro			\
	-Wl,-Ttext-segment=$(IMAGE_BASE_VIRTUAL)

TAGSFLAGS =				\
	-e				\
	-a				\
	--if0=no			\
	--langmap=c:.c.h.i		\
	--line-directives=yes

# some distros like alpine have a file /usr/include/fortify/string.h
# which wraps functions like memcpy() with inline branch traps which
# check for things like overlapping parameters and buffer overrun in
# situations where the array size is known. memcpy() is important in
# avoiding aliasing memory and proving no aliases exist so that code
# goes faster and is more portable. for example, here we use it when
# moving values to/from the cpu register file therefore the size and
# boundaries are always known, and as such, fortification needlessly
# causes an explosive growth in object code size in files like sse.c
CFLAGS += -U_FORTIFY_SOURCE

ifeq ($(USER), jart)
CFLAGS += -Wall -Werror -Wno-unused-function
endif

ifeq ($(MODE), cosmo)
CC = cosmocc
CFLAGS += -fno-pie
LDFLAGS += -no-pie
endif

ifeq ($(MODE), dbg)
CFLAGS += -O0 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer
CPPFLAGS += -DDEBUG
ifeq ($(HOST_SYSTEM), Linux)
CFLAGS += -fno-pie
LDFLAGS += -static -no-pie $(LDFLAGS_STATIC)
endif
ifneq ($(HOST_SYSTEM), Darwin)
ifneq ($(HOST_SYSTEM), FreeBSD)
CPPFLAGS += -DUNWIND
LDLIBS += -lunwind -llzma
endif
endif
endif

ifeq ($(MODE), rel)
CPPFLAGS += -DNDEBUG
CFLAGS += -O2 -mtune=generic -fno-align-functions -fno-align-jumps -fno-align-labels -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables
endif

ifeq ($(MODE), rel-llvm)
CC = clang
CPPFLAGS += -DNDEBUG
CFLAGS += -O2 -mtune=generic
endif

ifeq ($(MODE), tiny)
CPPFLAGS += -DNDEBUG -DTINY
CFLAGS += -Os -fomit-frame-pointer -mtune=generic -fno-align-functions -fno-align-jumps -fno-align-labels -fno-align-loops -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables
LDFLAGS += -no-pie #-Wl,--cref,-Map=$@.map
endif

ifeq ($(MODE), tiny-llvm)
CC = clang
CPPFLAGS += -DNDEBUG -DTINY
CFLAGS += -Oz
endif

ifeq ($(MODE), opt)
CPPFLAGS += -DNDEBUG
CFLAGS += -O3 -march=native
endif

ifeq ($(MODE), opt-llvm)
CC = clang
CPPFLAGS += -DNDEBUG
CFLAGS += -O2
TARGET_ARCH = -march=native
endif

# make m=prof o/prof/blink
# o/prof/blink/blink third_party/cosmo/mu_test.com
# gprof o/prof/blink/blink gmon.out | less
ifeq ($(MODE), prof)
CFLAGS += -pg
LDFLAGS += -pg
endif

# make m=cov check
# gcov -ukqjo o/cov/blink blink/*.c
# less alu.c.gcov
ifeq ($(MODE), cov)
CPPFLAGS += -DNOJIT
CFLAGS += -fprofile-arcs -ftest-coverage
LDFLAGS += -fprofile-arcs -ftest-coverage
endif

ifeq ($(MODE), asan)
CFLAGS += -O0
CPPFLAGS += -DDEBUG
CPPFLAGS += -fsanitize=address
LDLIBS += -fsanitize=address
endif

ifeq ($(MODE), ubsan)
CPPFLAGS += -DDEBUG -DNOJIT -DUBSAN
CFLAGS += -Werror -Wno-unused-parameter -Wno-missing-field-initializers
CFLAGS += -fsanitize=undefined
LDLIBS += -fsanitize=undefined
endif

ifeq ($(MODE), tsan)
CC = clang++
CPPFLAGS += -DTSAN
CFLAGS += -xc++ -Werror -Wno-unused-parameter -Wno-missing-field-initializers
CFLAGS += -fsanitize=thread
LDLIBS += -fsanitize=thread
endif

ifeq ($(MODE), msan)
CC = clang
CPPFLAGS += -DDEBUG
CFLAGS += -Werror -Wno-unused-parameter -Wno-missing-field-initializers
CFLAGS += -fsanitize=memory
LDLIBS += -fsanitize=memory
endif

ifeq ($(MODE), llvm)
CC = clang
CPPFLAGS += -DDEBUG
CFLAGS += -Werror -Wno-unused-parameter -Wno-missing-field-initializers
LDFLAGS += --rtlib=compiler-rt
endif

ifeq ($(MODE), llvm++)
CC = clang++
CPPFLAGS += -DDEBUG
CFLAGS += -xc++ -Werror -Wno-unused-parameter -Wno-missing-field-initializers
LDFLAGS += --rtlib=compiler-rt
endif
