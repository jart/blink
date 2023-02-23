#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

PKGS += TEST_FUNC
TEST_FUNC_FILES = $(wildcard test/func/*)
TEST_FUNC_SRCS = $(filter %.c,$(TEST_FUNC_FILES))
TEST_FUNC_OBJS = $(TEST_FUNC_SRCS:%.c=o/$(MODE)/x86_64/%.o)
TEST_FUNC_BINS = $(TEST_FUNC_SRCS:%.c=o/$(MODE)/%.elf)
TEST_FUNC_COMS = $(TEST_FUNC_SRCS:%.c=o/$(MODE)/%.com)
TEST_FUNC_CHECKS = $(TEST_FUNC_SRCS:%.c=o/$(MODE)/%.com.ok)
TEST_FUNC_EMULATES = $(foreach ARCH,$(ARCHITECTURES),$(foreach SRC,$(TEST_FUNC_SRCS),$(SRC:%.c=o/$(MODE)/$(ARCH)/%.elf.emulates)))

TEST_FUNC_LINK =							\
		$(VM)							\
		o/third_party/gcc/x86_64/bin/x86_64-linux-musl-gcc	\
		-static							\
		-Wl,-z,max-page-size=65536				\
		-Wl,-z,common-page-size=65536				\
		$<							\
		-o $@

o/$(MODE)/test/func/%.com.ok:						\
		o/$(MODE)/test/func/%.com				\
		o/$(MODE)/blink/blink
	$<
	@touch $@

$(TEST_FUNC_OBJS): private CFLAGS = -O
$(TEST_FUNC_OBJS): private CPPFLAGS = -isystem.

.PRECIOUS: o/$(MODE)/test/func/%.elf
o/$(MODE)/test/func/%.elf:						\
		o/$(MODE)/x86_64/test/func/%.o				\
		o/third_party/gcc/x86_64/bin/x86_64-linux-musl-gcc	\
		$(VM)
	@mkdir -p $(@D)
	$(TEST_FUNC_LINK)

$(TEST_FUNC_OBJS): test/func/func.mk

.PRECIOUS: o/$(MODE)/test/func/%.com
o/$(MODE)/test/func/%.com:						\
		o/$(MODE)/test/func/%.elf				\
		o/$(MODE)/blink/blink
	@mkdir -p $(@D)
	@echo "#!/bin/sh" >$@
	@echo "echo [test] $(VM) $< >&2" >>$@
	@echo "$(VM) $< || exit" >>$@
	@echo "echo [test] o/$(MODE)/blink/blink $< >&2" >>$@
	@echo "o/$(MODE)/blink/blink $< || exit" >>$@
	@echo "echo [test] o/$(MODE)/blink/blink -jm $< >&2" >>$@
	@echo "o/$(MODE)/blink/blink -jm $< || exit" >>$@
	@echo "echo [test] o/$(MODE)/blink/blink -m $< >&2" >>$@
	@echo "o/$(MODE)/blink/blink -m $< || exit" >>$@
	@echo "echo [test] o/$(MODE)/blink/blink -j $< >&2" >>$@
	@echo "o/$(MODE)/blink/blink -j $< || exit" >>$@
	@chmod +x $@

.PHONY: o/$(MODE)/test/func
o/$(MODE)/test/func:							\
	$(TEST_FUNC_CHECKS)

.PHONY: o/$(MODE)/test/func/emulates
o/$(MODE)/test/func/emulates:						\
	$(TEST_FUNC_EMULATES)
