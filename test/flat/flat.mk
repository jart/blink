#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

TEST_FLAT_FILES = $(wildcard test/flat/*)
TEST_FLAT_SRCS = $(filter %.S,$(TEST_FLAT_FILES))
TEST_FLAT_OBJS = $(TEST_FLAT_SRCS:%.S=o/$(MODE)/x86_64/%.o)
TEST_FLAT_BINS = $(TEST_FLAT_SRCS:%.S=o/$(MODE)/%.bin)
TEST_FLAT_CHECKS = $(TEST_FLAT_BINS:%=%.ok)

TEST_FLAT_LINK =							\
		$(VM)							\
		o/third_party/gcc/x86_64/bin/x86_64-linux-musl-ld.bfd	\
		-T test/flat/flat.lds					\
		-static							\
		$<							\
		-o $@

.PRECIOUS: o/$(MODE)/test/flat/%.bin
o/$(MODE)/test/flat/%.bin:						\
		o/$(MODE)/x86_64/test/flat/%.o				\
		o/third_party/gcc/x86_64/bin/x86_64-linux-musl-gcc	\
		test/flat/flat.lds
	@mkdir -p $(@D)
	$(TEST_FLAT_LINK)

$(TEST_FLAT_OBJS): test/flat/flat.mk

# the .com extension is for cosmopolitan/tool/emacs/ integration
o/$(MODE)/test/flat/%.com:						\
		o/$(MODE)/test/flat/%.bin				\
		o/$(MODE)/blink/blink
	@echo "#!/bin/sh" >$@
	@echo "o/$(MODE)/blink/blink $< || exit" >>$@
	@chmod +x $@

o/$(MODE)/test/flat/%.bin.ok: o/$(MODE)/test/flat/%.bin o/$(MODE)/blink/blink
	@mkdir -p $(@D)
	o/$(MODE)/blink/blink $<
	@touch $@

o/$(MODE)/test/flat:							\
	$(TEST_FLAT_CHECKS)
