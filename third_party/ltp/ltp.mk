#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

.PRECIOUS: third_party/ltp/%.gz
third_party/ltp/%.gz: third_party/ltp/%.gz.sha256 o/tool/sha256sum
	curl -so $@ https://justine.storage.googleapis.com/ltp/$(subst third_party/ltp/,,$@)
	o/tool/sha256sum -c $<

.PRECIOUS: third_party/ltp/bin/%.elf
third_party/ltp/bin/%.elf: third_party/ltp/bin/%.elf.gz
	gzip -dc <$< >$@
	chmod +x $@

o/$(MODE)/third_party/ltp/bin/%.elf.ok:			\
		third_party/ltp/bin/%.elf		\
		o/$(MODE)/blink/blink			\
		o/lib/ld-musl-x86_64.so.1		\
		$(VM)
	@mkdir -p $(@D)
	o/$(MODE)/blink/blink $<
	@touch $@

LTP_TESTS += o//third_party/ltp/bin/1/close_range02.elf.ok

ifneq ($(HOST_OS), Cygwin)
# partial failure only on cygwin
# chmod01.c:50: TFAIL: stat(testfile) mode=0000
# chmod01.c:50: TFAIL: stat(testdir_1) mode=0000
# chmod01.c:50: TFAIL: stat(testdir_1) mode=0777
# chmod01.c:50: TFAIL: stat(testdir_1) mode=4777
LTP_TESTS += o//third_party/ltp/bin/1/chmod01.elf.ok
endif

o/$(MODE)/third_party/ltp: $(LTP_TESTS)
	@mkdir -p $(@D)
	@touch $@
