#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

TEST_METALROM_FILES := $(wildcard test/metalrom/*)
TEST_METALROM_SRCS = $(filter %.S,$(TEST_METALROM_FILES))
TEST_METALROM_HDRS = $(filter %.h,$(TEST_METALROM_FILES))
TEST_METALROM_OBJS = $(TEST_METALROM_SRCS:%.S=o/$(MODE)/x86_64-gcc49/%.o)
TEST_METALROM_BINS = $(TEST_METALROM_SRCS:%.S=o/$(MODE)/%.bin)
TEST_METALROM_CHECKS = $(TEST_METALROM_BINS:%=%.ok)

TEST_METALROM_LINK =								\
		$(VM)								\
		o/third_party/gcc/x86_64-gcc49/bin/x86_64-linux-musl-ld.bfd	\
		-T test/metalrom/metalrom.lds					\
		-static								\
		$<								\
		-o $@

.PRECIOUS: o/$(MODE)/test/metalrom/%.bin
o/$(MODE)/test/metalrom/%.bin:							\
		o/$(MODE)/x86_64-gcc49/test/metalrom/%.o			\
		o/third_party/gcc/x86_64-gcc49/bin/x86_64-linux-musl-gcc	\
		test/metalrom/metalrom.lds					\
		$(VM)
	@mkdir -p $(@D)
	$(TEST_METALROM_LINK)

$(TEST_METALROM_OBJS):							\
		test/metalrom/metalrom.mk				\
		$(TEST_METALROM_HDRS)					\
		$(BLINK_HDRS)

o/$(MODE)/test/metalrom/%.bin.ok:					\
		o/$(MODE)/test/metalrom/%.bin				\
		o/$(MODE)/blink/blinkenlights				\
		third_party/sectorlisp/sectorlisp-friendly.bin
	@mkdir -p $(@D)
	o/$(MODE)/blink/blinkenlights					\
		-r							\
		-t							\
		-B $<							\
		third_party/sectorlisp/sectorlisp-friendly.bin
	@touch $@

o/$(MODE)/test/metalrom:							\
	$(TEST_METALROM_CHECKS)
