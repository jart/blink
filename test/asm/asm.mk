#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

TEST_ASM_FILES = $(wildcard test/asm/*)
TEST_ASM_SRCS = $(filter %.S,$(TEST_ASM_FILES))
TEST_ASM_OBJS = $(TEST_ASM_SRCS:%.S=o/$(MODE)/x86_64/%.o)
TEST_ASM_BINS = $(TEST_ASM_SRCS:%.S=o/$(MODE)/%.elf)
TEST_ASM_COMS = $(TEST_ASM_SRCS:%.S=o/$(MODE)/%.com)
TEST_ASM_CHECKS = $(TEST_ASM_SRCS:%.S=o/$(MODE)/%.com.ok)
TEST_ASM_EMULATES = $(foreach ARCH,$(ARCHITECTURES),$(foreach SRC,$(TEST_ASM_SRCS),$(SRC:%.S=o/$(MODE)/$(ARCH)/%.elf.emulates)))

TEST_ASM_LINK =	$(VM)							\
		o/third_party/gcc/x86_64/bin/x86_64-linux-musl-ld.bfd	\
		-static							\
		$<							\
		-o $@

o/$(MODE)/test/asm/%.com.ok:						\
		o/$(MODE)/test/asm/%.com				\
		o/$(MODE)/blink/blink
	$<
	@touch $@

.PRECIOUS: o/$(MODE)/test/asm/%.elf
o/$(MODE)/test/asm/%.elf:						\
		o/$(MODE)/x86_64/test/asm/%.o				\
		o/third_party/gcc/x86_64/bin/x86_64-linux-musl-gcc
	@mkdir -p $(@D)
	$(TEST_ASM_LINK)

$(TEST_ASM_OBJS): test/asm/asm.mk test/asm/mac.inc

# the .com extension is for cosmo/tool/emacs/ integration
.PRECIOUS: o/$(MODE)/test/asm/%.com
o/$(MODE)/test/asm/%.com:						\
		o/$(MODE)/test/asm/%.elf				\
		o/$(MODE)/blink/blink
	@mkdir -p $(@D)
	@echo "#!/bin/sh" >$@
	@echo "echo testing $(VM) $<" >>$@
	@echo "$(VM) $< || exit" >>$@
	@echo "echo testing o/$(MODE)/blink/blink $<" >>$@
	@echo "o/$(MODE)/blink/blink $< || exit" >>$@
	@chmod +x $@

.PHONY: o/$(MODE)/test/asm
o/$(MODE)/test/asm:							\
	$(TEST_ASM_CHECKS)

.PHONY: o/$(MODE)/test/asm/emulates
o/$(MODE)/test/asm/emulates:						\
	$(TEST_ASM_EMULATES)
