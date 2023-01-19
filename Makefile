#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

SHELL = /bin/sh
MAKEFLAGS += --no-builtin-rules
ARCHITECTURES = x86_64 x86_64-gcc49 i486 aarch64 riscv64 arm mips s390x mipsel mips64 mips64el powerpc powerpc64le

.SUFFIXES:
.DELETE_ON_ERROR:
.FEATURES: output-sync
.PHONY: o all clean check check2 test tags

ifneq ($(m),)
ifeq ($(MODE),)
MODE := $(m)
endif
endif

HOST_SYSTEM := $(shell uname -s)
HOST_ARCH := $(shell uname -m)
HOST_OS := $(shell uname -o 2>/dev/null)

ifneq ($(HOST_SYSTEM), Linux)
VM = o/$(MODE)/blink/blink
endif
ifneq ($(HOST_ARCH), x86_64)
VM = o/$(MODE)/blink/blink
endif

o:	o/$(MODE)/blink

test:	o				\
	o/$(MODE)/test

check:	test				\
	o/$(MODE)/third_party/cosmo

check2:	o/$(MODE)/test/sse		\
	o/$(MODE)/test/lib		\
	o/$(MODE)/test/sys		\
	o/$(MODE)/test/func		\
	o/$(MODE)/test/asm		\
	o/$(MODE)/test/asm/emulates	\
	o/$(MODE)/test/func/emulates

emulates:				\
	o/$(MODE)/test/asm		\
	o/$(MODE)/test/flat		\
	o/$(MODE)/third_party/cosmo/emulates

tags: TAGS HTAGS

clean:
	rm -rf o

include build/config.mk
include build/rules.mk
include third_party/zlib/zlib.mk
include blink/blink.mk
include test/test.mk
include test/asm/asm.mk
include test/func/func.mk
include test/flat/flat.mk
include test/blink/test.mk
include third_party/gcc/gcc.mk
include third_party/qemu/qemu.mk
include third_party/cosmo/cosmo.mk

OBJS	 = $(foreach x,$(PKGS),$($(x)_OBJS))
SRCS	:= $(foreach x,$(PKGS),$($(x)_SRCS))
HDRS	:= $(foreach x,$(PKGS),$($(x)_HDRS))
INCS	 = $(foreach x,$(PKGS),$($(x)_INCS))
BINS	 = $(foreach x,$(PKGS),$($(x)_BINS))
TESTS	 = $(foreach x,$(PKGS),$($(x)_TESTS))
CHECKS	 = $(foreach x,$(PKGS),$($(x)_CHECKS))

o/$(MODE)/.x:
	@mkdir -p $(@D)
	@touch $@

o/tool/sha256sum: tool/sha256sum.c
	@mkdir -p $(@D)
	$(CC) -w -O2 -o $@ $<

o/$(MODE)/tool/sha256sum.o: tool/sha256sum.c
	@mkdir -p $(@D)
	clang++ -Wall -Wextra -Werror -pedantic -O2 -xc++ -c -o $@ $<
	g++ -Wall -Wextra -Werror -pedantic -O2 -xc++ -c -o $@ $<
	clang -Wall -Wextra -Werror -pedantic -O2 -c -o $@ $<
	gcc -Wall -Wextra -Werror -pedantic -O2 -c -o $@ $<

o/$(MODE)/srcs.txt: o/$(MODE)/.x $(MAKEFILES) $(SRCS) $(call uniq,$(foreach x,$(SRCS),$(dir $(x))))
	$(file >$@) $(foreach x,$(SRCS),$(file >>$@,$(x)))
o/$(MODE)/hdrs.txt: o/$(MODE)/.x $(MAKEFILES) $(HDRS) $(call uniq,$(foreach x,$(HDRS) $(INCS),$(dir $(x))))
	$(file >$@) $(foreach x,blink/types.h $(HDRS) $(INCS),$(file >>$@,$(x)))

DEPENDS =				\
	o/$(MODE)/depend.host		\
	o/$(MODE)/depend.i486		\
	o/$(MODE)/depend.m68k		\
	o/$(MODE)/depend.x86_64		\
	o/$(MODE)/depend.x86_64-gcc49	\
	o/$(MODE)/depend.arm		\
	o/$(MODE)/depend.aarch64	\
	o/$(MODE)/depend.riscv64	\
	o/$(MODE)/depend.mips		\
	o/$(MODE)/depend.mipsel		\
	o/$(MODE)/depend.mips64		\
	o/$(MODE)/depend.mips64el	\
	o/$(MODE)/depend.s390x		\
	o/$(MODE)/depend.microblaze	\
	o/$(MODE)/depend.powerpc	\
	o/$(MODE)/depend.powerpc64le

o/$(MODE)/depend: $(DEPENDS)
	cat $^ >$@
o/$(MODE)/depend.host: o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	$(VM) build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt 2>/dev/null
o/$(MODE)/depend.i486: o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	$(VM) build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/i486/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt 2>/dev/null
o/$(MODE)/depend.m68k: o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	$(VM) build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/m68k/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt 2>/dev/null
o/$(MODE)/depend.x86_64: o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	$(VM) build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/x86_64/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt 2>/dev/null
o/$(MODE)/depend.x86_64-gcc49: o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	$(VM) build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/x86_64-gcc49/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt 2>/dev/null
o/$(MODE)/depend.arm: o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	$(VM) build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/arm/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt 2>/dev/null
o/$(MODE)/depend.aarch64: o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	$(VM) build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/aarch64/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt 2>/dev/null
o/$(MODE)/depend.riscv64: o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	$(VM) build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/riscv64/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt 2>/dev/null
o/$(MODE)/depend.mips: o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	$(VM) build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/mips/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt 2>/dev/null
o/$(MODE)/depend.mipsel: o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	$(VM) build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/mipsel/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt 2>/dev/null
o/$(MODE)/depend.mips64: o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	$(VM) build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/mips64/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt 2>/dev/null
o/$(MODE)/depend.mips64el: o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	$(VM) build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/mips64el/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt 2>/dev/null
o/$(MODE)/depend.s390x: o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	$(VM) build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/s390x/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt 2>/dev/null
o/$(MODE)/depend.microblaze: o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	$(VM) build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/microblaze/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt 2>/dev/null
o/$(MODE)/depend.powerpc: o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	$(VM) build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/powerpc/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt 2>/dev/null
o/$(MODE)/depend.powerpc64le: o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	$(VM) build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/powerpc64le/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt 2>/dev/null

TAGS:	o/$(MODE)/srcs.txt $(SRCS)
	$(RM) $@
	$(TAGS) $(TAGSFLAGS) -L $< -o $@

HTAGS:	o/$(MODE)/hdrs.txt $(HDRS)
	$(RM) $@
	build/htags -L $< -o $@

$(SRCS):
$(HDRS):
$(INCS):
.DEFAULT:
	@echo >&2
	@echo NOTE: deleting o/$(MODE)/depend because of an unspecified prerequisite: $@ >&2
	@echo >&2
	rm -f o/$(MODE)/depend $(DEPENDS)

-include o/$(MODE)/depend
