#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

TEST_ASM_FILES := $(wildcard test/asm/*)
TEST_ASM_SRCS = $(filter %.S,$(TEST_ASM_FILES))
TEST_ASM_OBJS = $(TEST_ASM_SRCS:%.S=o/$(MODE)/x86_64-gcc49/%.o)
TEST_ASM_BINS = $(TEST_ASM_SRCS:%.S=o/$(MODE)/%.elf)
TEST_ASM_COMS = $(TEST_ASM_SRCS:%.S=o/$(MODE)/%.com)
TEST_ASM_CHECKS = $(TEST_ASM_SRCS:%.S=o/$(MODE)/%.com.ok)
TEST_ASM_EMULATES = $(foreach ARCH,$(ARCHITECTURES),$(foreach SRC,$(TEST_ASM_SRCS),$(SRC:%.S=o/$(MODE)/$(ARCH)/%.elf.emulates)))

TEST_ASM_LINK =	$(VM)								\
		o/third_party/gcc/x86_64-gcc49/bin/x86_64-linux-musl-ld.bfd	\
		-static								\
		--omagic							\
		-z noexecstack							\
		-z max-page-size=65536						\
		-z common-page-size=65536					\
		$<								\
		-o $@

o/$(MODE)/test/asm/%.com.ok:							\
		o/$(MODE)/test/asm/%.com
	$<
	@touch $@

.PRECIOUS: o/$(MODE)/test/asm/%.elf
o/$(MODE)/test/asm/%.elf:							\
		o/$(MODE)/x86_64-gcc49/test/asm/%.o				\
		o/third_party/gcc/x86_64-gcc49/bin/x86_64-linux-musl-gcc	\
		$(VM)
	@mkdir -p $(@D)
	$(TEST_ASM_LINK)

$(TEST_ASM_OBJS): test/asm/asm.mk test/asm/mac.inc

# the .com extension is for cosmo/tool/emacs/ integration
.PRECIOUS: o/$(MODE)/test/asm/%.com
o/$(MODE)/test/asm/%.com:						\
		o/$(MODE)/test/asm/%.elf				\
		o/$(MODE)/blink/blink					\
		o/third_party/qemu/4/qemu-x86_64			\
		$(VM)
	@mkdir -p $(@D)
	@echo "#!/bin/sh" >$@
	@echo "echo [test] $(VM) $< >&2" >>$@
	@echo "$(VM) $< || exit" >>$@
	@echo "echo [test] o/$(MODE)/blink/blink $< >&2" >>$@
	@echo "o/$(MODE)/blink/blink $< || exit" >>$@
	@echo "echo [test] o/$(MODE)/blink/blink -m $< >&2" >>$@
	@echo "o/$(MODE)/blink/blink -m $< || exit" >>$@
	@echo "echo [test] o/$(MODE)/blink/blink -jm $< >&2" >>$@
	@echo "o/$(MODE)/blink/blink -jm $< || exit" >>$@
	@echo "echo [test] o/third_party/qemu/4/qemu-x86_64 -cpu core2duo $< >&2" >>$@
	@echo "$(VM) o/third_party/qemu/4/qemu-x86_64 -cpu core2duo $< || exit" >>$@
	@chmod +x $@

.PHONY: o/$(MODE)/test/asm
o/$(MODE)/test/asm:							\
	$(TEST_ASM_CHECKS)

.PHONY: o/$(MODE)/test/asm/emulates
o/$(MODE)/test/asm/emulates:						\
	$(TEST_ASM_EMULATES)
