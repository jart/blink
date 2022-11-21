#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

TEST_ASM_FILES = $(wildcard test/asm/*)
TEST_ASM_SRCS = $(filter %.S,$(TEST_ASM_FILES))
TEST_ASM_OBJS = $(TEST_ASM_SRCS:%.S=o/$(MODE)/x86_64/%.o)
TEST_ASM_BINS = $(TEST_ASM_OBJS:%.o=%.elf)
TEST_ASM_CHECKS = $(TEST_ASM_BINS:%.elf=%.elf.ok)

TEST_ASM_LINK =	$(VM)							\
		o/third_party/gcc/x86_64/bin/x86_64-linux-musl-ld.bfd	\
		-static							\
		$<							\
		-o $@

o/$(MODE)/x86_64/test/asm/%.elf.ok:					\
		o/$(MODE)/x86_64/test/asm/%.elf				\
		o/$(MODE)/blink/blink
	o/$(MODE)/blink/blink $<
	@touch $@

o/$(MODE)/x86_64/test/asm/%.elf:					\
		o/$(MODE)/x86_64/test/asm/%.o				\
		o/$(MODE)/x86_64/test/asm/elf.o				\
		o/third_party/gcc/x86_64/bin/x86_64-linux-musl-ld.bfd	\
		test/asm/asm.lds
	$(TEST_ASM_LINK)

$(TEST_ASM_OBJS): test/asm/asm.mk test/asm/mac.inc

o/$(MODE)/test/asm:							\
	$(TEST_ASM_OBJS)						\
	$(TEST_ASM_BINS)						\
	$(TEST_ASM_CHECKS)

################################################################################
# for cosmo/tool/emacs/ integration

o/$(MODE)/test/asm/%.com:						\
		o/$(MODE)/x86_64/test/asm/%.elf				\
		o/$(MODE)/blink/blink
	echo "#!/bin/sh" >$@
	echo "echo testing $<" >>$@
	echo "$(VM) $< || exit" >>$@
	echo "echo testing o/$(MODE)/blink/blink $<" >>$@
	echo "$(VM) o/$(MODE)/blink/blink $< || exit" >>$@
	chmod +x $@
