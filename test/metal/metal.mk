#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

TEST_METAL_FILES = $(wildcard test/metal/*)
TEST_METAL_SRCS = $(filter %.S,$(TEST_METAL_FILES))
TEST_METAL_OBJS = $(TEST_METAL_SRCS:%.S=o/$(MODE)/x86_64/%.o)
TEST_METAL_BINS = $(TEST_METAL_SRCS:%.S=o/$(MODE)/%.bin)
TEST_METAL_CHECKS = $(TEST_METAL_BINS:%=%.ok)

TEST_METAL_LINK =							\
		$(VM)							\
		o/third_party/gcc/x86_64/bin/x86_64-linux-musl-ld.bfd	\
		-T test/metal/metal.lds					\
		-static							\
		$<							\
		-o $@

.PRECIOUS: o/$(MODE)/test/metal/%.bin
o/$(MODE)/test/metal/%.bin:						\
		o/$(MODE)/x86_64/test/metal/%.o				\
		o/third_party/gcc/x86_64/bin/x86_64-linux-musl-gcc	\
		test/metal/metal.lds					\
		$(VM)
	@mkdir -p $(@D)
	$(TEST_METAL_LINK)

$(TEST_METAL_OBJS): test/metal/metal.mk

o/$(MODE)/test/metal/%.bin.ok:						\
		o/$(MODE)/test/metal/%.bin				\
		o/$(MODE)/blink/blinkenlights
	@mkdir -p $(@D)
	o/$(MODE)/blink/blinkenlights -r -t $<
	@touch $@

o/$(MODE)/test/metal:							\
	$(TEST_METAL_CHECKS)
