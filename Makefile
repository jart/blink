#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

SHELL = /bin/sh
MAKEFLAGS += --no-builtin-rules
ARCHITECTURES = x86_64 x86_64-gcc49 i486 aarch64 arm mips s390x mipsel mips64 mips64el powerpc powerpc64le

.SUFFIXES:
.DELETE_ON_ERROR:
.FEATURES: output-sync
.PHONY: o all clean check check2 test tags format install

ifeq ($(MAKE_VERSION), 3.81)
$(error please "brew install make" and use the "gmake" command)
endif
ifneq ($(shell echo -e "4.0\n$(MAKE_VERSION)" | sort -ct. -k1,1n -k2,2n 2>/dev/null && echo YES), YES)
$(error please use GNU Make 4.0 or newer)
endif

ifeq ($(wildcard config.h),)
  $(error ./configure needs to be run, use ./configure --help for help)
endif
include config.mk
CONFIG_COMMAND ?= ./configure
ifneq ($(CONFIG_HOSTNAME), $(shell hostname))
  undefine CC
  undefine AR
  undefine TMPDIR
  undefine CFLAGS
  undefine LDFLAGS
  undefine UOPFLAGS
  undefine CPPFLAGS
  undefine TARGET_ARCH
  $(shell $(CONFIG_COMMAND))
  $(error please run $(MAKE) again)
endif

ifneq ($(m),)
ifeq ($(MODE),)
MODE := $(m)
endif
endif

ifneq ($(HOST_SYSTEM), Linux)
VM = o/$(MODE)/blink/blink
endif
ifneq ($(HOST_ARCH), x86_64)
VM = o/$(MODE)/blink/blink
endif

o:	o/ok
o/ok:	o/$(MODE)/blink
	touch $@
	@echo ""
	@echo "Your Blink Virtual Machine successfully compiled:"
	@echo ""
	@echo "  o/$(MODE)/blink/blink"
	@echo "  o/$(MODE)/blink/blinkenlights"
	@echo ""
	@echo "You may also want to run:"
	@echo ""
	@echo "  make check"
	@echo "  doas make install"
	@echo ""

test:	o/$(MODE)/blink				\
	o/$(MODE)/test

format:
	clang-format -i -style=file blink/*.c blink/*.h

check:	test					\
	o/$(MODE)/third_party/cosmo		\
	o/$(MODE)/third_party/libc-test		\
	o/$(MODE)/test/metal

check2:	o/$(MODE)/test/sse			\
	o/$(MODE)/test/lib			\
	o/$(MODE)/test/sys			\
	o/$(MODE)/test/func			\
	o/$(MODE)/test/asm			\
	o/$(MODE)/third_party/ltp		\
	o/$(MODE)/test/asm/emulates		\
	o/$(MODE)/test/func/emulates

emulates:					\
	o/$(MODE)/test/asm			\
	o/$(MODE)/test/flat			\
	o/$(MODE)/third_party/ltp/medium	\
	o/$(MODE)/third_party/cosmo/emulates

tags: TAGS HTAGS

include build/config.mk
include build/rules.mk
include third_party/libz/zlib.mk
include blink/blink.mk
include test/test.mk
include test/asm/asm.mk
include test/func/func.mk
include test/flat/flat.mk
include test/blink/test.mk
include test/metal/metal.mk
include tool/config/config.mk
include third_party/gcc/gcc.mk
include third_party/ltp/ltp.mk
include third_party/musl/musl.mk
include third_party/qemu/qemu.mk
include third_party/cosmo/cosmo.mk
include third_party/libc-test/libc-test.mk

BUILD_TOOLCHAIN := -DBUILD_TOOLCHAIN="\"$(shell $(CC) --version | head -n1)\""
BUILD_TIMESTAMP := -DBUILD_TIMESTAMP="\"$(shell LC_ALL=C TZ=UTC date +"%a %b %e %T %Z %Y")\""
BLINK_COMMITS := -DBLINK_COMMITS="\"$(shell git rev-list HEAD --count 2>/dev/null)\""
BLINK_GITSHA := -DBLINK_GITSHA="\"$(shell git rev-parse --verify HEAD 2>/dev/null)\""

CONFIG_CPPFLAGS =			\
	$(CONFIG_ARGUMENTS)		\
	$(BUILD_TOOLCHAIN)		\
	$(BUILD_TIMESTAMP)		\
	$(BLINK_COMMITS)		\
	$(BLINK_UNAME_V)		\
	$(BLINK_GITSHA)			\
	-DBUILD_MODE="\"$(MODE)\""

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

MAKEFILES =				\
	Makefile			\
	build/config.mk			\
	build/rules.mk			\
	blink/blink.mk			\
	third_party/libz/zlib.mk

$(OBJS): $(MAKEFILES)

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

$(OBJS): config.h

config.h: configure
	$(CONFIG_COMMAND)

install:
	mkdir -p $(PREFIX)/bin
	install -m 0755 o//blink/blink $(PREFIX)/bin/blink
	install -m 0755 o//blink/blinkenlights $(PREFIX)/bin/blinkenlights
	mkdir -p $(PREFIX)/share/man/man1
	install -m 0644 blink/blink.1 $(PREFIX)/share/man/man1/blink.1
	install -m 0644 blink/blinkenlights.1 $(PREFIX)/share/man/man1/blinkenlights.1

clean:
	rm -f $(OBJS) o/$(MODE)/blink/blink o/$(MODE)/blink/blinkenlights o/$(MODE)/blink/blink.a o/$(MODE)/third_party/libz/zlib.a

distclean:
	rm -rf o
	rm -f config.h TAGS HTAGS *.dump gmon.out perf.data perf.data.old
	rm -f $(find third_party -name \*.elf)
	rm -f $(find third_party -name \*.com)
	rm -f $(find third_party -name \*.dbg)
	rm -f $(find third_party -name \*.so.1)
	rm -f $(find third_party -name \*.so)
	rm -f $(find third_party -name \*.gz)
	rm -f $(find third_party -name \*.xz)

$(SRCS):
$(HDRS):
$(INCS):
.DEFAULT:
	@echo >&2
	@echo NOTE: deleting o/$(MODE)/depend because of an unspecified prerequisite: $@ >&2
	@echo >&2
	rm -f o/$(MODE)/depend $(DEPENDS)

-include o/$(MODE)/depend
