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
		$<							\
		-o $@

o/$(MODE)/test/func/%.com.ok:						\
		o/$(MODE)/test/func/%.com				\
		o/$(MODE)/blink/blink
	$<
	@touch $@

.PRECIOUS: o/$(MODE)/test/func/%.elf
o/$(MODE)/test/func/%.elf:						\
		o/$(MODE)/x86_64/test/func/%.o				\
		o/third_party/gcc/x86_64/bin/x86_64-linux-musl-gcc	\
		$(VM)
	@mkdir -p $(@D)
	$(TEST_FUNC_LINK)

$(TEST_FUNC_OBJS): test/func/func.mk

# the .com extension is for cosmo/tool/emacs/ integration
.PRECIOUS: o/$(MODE)/test/func/%.com
o/$(MODE)/test/func/%.com:						\
		o/$(MODE)/test/func/%.elf				\
		o/$(MODE)/blink/blink
	@mkdir -p $(@D)
	@echo "#!/bin/sh" >$@
	@echo "echo o/$(MODE)/blink/blink $< >&2" >>$@
	@echo "o/$(MODE)/blink/blink $<" >>$@
	@chmod +x $@

.PHONY: o/$(MODE)/test/func
o/$(MODE)/test/func:							\
	$(TEST_FUNC_CHECKS)

.PHONY: o/$(MODE)/test/func/emulates
o/$(MODE)/test/func/emulates:						\
	$(TEST_FUNC_EMULATES)
