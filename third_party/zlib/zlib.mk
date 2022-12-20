#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

PKGS += ZLIB
ZLIB_FILES := $(wildcard third_party/zlib/*)
ZLIB_SRCS = $(filter %.c,$(ZLIB_FILES))
ZLIB_HDRS = $(filter %.h,$(ZLIB_FILES))
ZLIB_OBJS = $(ZLIB_SRCS:%.c=o/$(MODE)/%.o)

o/$(MODE)/third_party/zlib/zlib.a: $(ZLIB_OBJS)

o/$(MODE)/third_party/zlib:	\
	o/$(MODE)/third_party/zlib/zlib.a
