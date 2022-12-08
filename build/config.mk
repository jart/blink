#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

TAGS ?= /usr/bin/ctags

IMAGE_BASE_VIRTUAL = 0x23000000
# IMAGE_BASE_VIRTUAL = 0x7e0000000000

CFLAGS +=				\
	-g				\
	-O2				\
	-pthread			\
	-fno-ident			\
	-fno-common			\
	-fstrict-aliasing		\
	-fstrict-overflow

CPPFLAGS +=				\
	-iquote.			\
	-D_POSIX_C_SOURCE=200809L	\
	-D_XOPEN_SOURCE=700		\
	-D_DARWIN_C_SOURCE		\
	-D_DEFAULT_SOURCE		\
	-D_BSD_SOURCE			\
	-D_GNU_SOURCE			\
	-D__BSD_VISIBLE

LDLIBS +=				\
	-lm				\
	-pthread

LDFLAGS_STATIC =			\
	-static				\
	-Wl,-z,norelro			\
	-Wl,-z,max-page-size=4096	\
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
CFLAGS +=				\
	-U_FORTIFY_SOURCE

ifeq ($(USER), jart)
CFLAGS +=				\
	-fno-stack-protector		\
	-Wall				\
	-Werror				\
	-Wno-unused-function		\
	-Wno-unused-const-variable
endif

ifeq ($(MODE), rel)
CPPFLAGS += -DNDEBUG
CFLAGS += -O2 -mtune=generic
endif

ifeq ($(MODE), opt)
CPPFLAGS += -DNDEBUG
CFLAGS += -O3 -march=native
endif

# ifeq ($(MODE), opt)
# CC = clang
# # CPPFLAGS += -DNDEBUG
# CFLAGS += -O3
# TARGET_ARCH = -march=native
# endif

ifeq ($(MODE), dbg)
CFLAGS += -O0
CPPFLAGS += -DDEBUG
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
CC = clang
CPPFLAGS += -DDEBUG
CFLAGS += -Werror -Wno-unused-parameter -Wno-missing-field-initializers
CFLAGS += -fsanitize=undefined
LDLIBS += -fsanitize=undefined
endif

ifeq ($(MODE), tsan)
CC = clang++
CPPFLAGS +=
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

ifeq ($(MODE), tiny)
CPPFLAGS += -DNDEBUG -DTINY
CFLAGS += -Os -mtune=generic -fno-align-functions -fno-align-jumps -fno-align-labels -fno-align-loops -fno-pie
LDFLAGS += -no-pie -Wl,--cref,-Map=$@.map
endif

# ifeq ($(MODE), tiny)
# CC = clang
# CPPFLAGS += -DNDEBUG -DTINY
# CFLAGS += -Oz -fno-pie
# LDFLAGS += -no-pie -Wl,--cref,-Map=$@.map
# endif

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
