#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

SHELL = /bin/sh
MAKEFLAGS += --no-builtin-rules

.SUFFIXES:
.DELETE_ON_ERROR:
.FEATURES: output-sync
.PHONY: o clean

ifneq ($(m),)
ifeq ($(MODE),)
MODE := $(m)
endif
endif

o: o/$(MODE)/blink
test: o/$(MODE)/test
clean:; rm -rf o
tags: TAGS HTAGS

include build/config.mk
include build/rules.mk
include blink/blink.mk
include test/test.mk
include test/blink/test.mk
include third_party/gcc/gcc.mk
include third_party/qemu/qemu.mk

OBJS	 = $(foreach x,$(PKGS),$($(x)_OBJS))
SRCS	:= $(foreach x,$(PKGS),$($(x)_SRCS))
HDRS	:= $(foreach x,$(PKGS),$($(x)_HDRS))
INCS	 = $(foreach x,$(PKGS),$($(x)_INCS))
BINS	 = $(foreach x,$(PKGS),$($(x)_BINS))
TESTS	 = $(foreach x,$(PKGS),$($(x)_TESTS))
CHECKS	 = $(foreach x,$(PKGS),$($(x)_CHECKS))

o/$(MODE)/.x:
	mkdir -p $(@D)
	touch $@
o/$(MODE)/srcs.txt: o/$(MODE)/.x $(MAKEFILES) $(call uniq,$(foreach x,$(SRCS),$(dir $(x))))
	$(file >$@) $(foreach x,$(SRCS),$(file >>$@,$(x)))
o/$(MODE)/hdrs.txt: o/$(MODE)/.x $(MAKEFILES) $(call uniq,$(foreach x,$(HDRS) $(INCS),$(dir $(x))))
	$(file >$@) $(foreach x,$(HDRS) $(INCS),$(file >>$@,$(x)))

DEPENDS =				\
	o/$(MODE)/depend.host		\
	o/$(MODE)/depend.i486		\
	o/$(MODE)/depend.m68k		\
	o/$(MODE)/depend.x86_64		\
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
o/$(MODE)/depend.host: build/bootstrap/mkdeps.com o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt
o/$(MODE)/depend.i486: build/bootstrap/mkdeps.com o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/i486/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt
o/$(MODE)/depend.m68k: build/bootstrap/mkdeps.com o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/m68k/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt
o/$(MODE)/depend.x86_64: build/bootstrap/mkdeps.com o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/x86_64/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt
o/$(MODE)/depend.arm: build/bootstrap/mkdeps.com o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/arm/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt
o/$(MODE)/depend.aarch64: build/bootstrap/mkdeps.com o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/aarch64/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt
o/$(MODE)/depend.riscv64: build/bootstrap/mkdeps.com o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/riscv64/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt
o/$(MODE)/depend.mips: build/bootstrap/mkdeps.com o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/mips/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt
o/$(MODE)/depend.mipsel: build/bootstrap/mkdeps.com o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/mipsel/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt
o/$(MODE)/depend.mips64: build/bootstrap/mkdeps.com o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/mips64/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt
o/$(MODE)/depend.mips64el: build/bootstrap/mkdeps.com o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/mips64el/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt
o/$(MODE)/depend.s390x: build/bootstrap/mkdeps.com o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/s390x/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt
o/$(MODE)/depend.microblaze: build/bootstrap/mkdeps.com o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/microblaze/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt
o/$(MODE)/depend.powerpc: build/bootstrap/mkdeps.com o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/powerpc/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt
o/$(MODE)/depend.powerpc64le: build/bootstrap/mkdeps.com o/$(MODE)/srcs.txt o/$(MODE)/hdrs.txt
	build/bootstrap/mkdeps.com -o $@ -r o/$(MODE)/powerpc64le/ @o/$(MODE)/srcs.txt @o/$(MODE)/hdrs.txt

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
