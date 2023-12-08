#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi ──────────────────────┘

third_party/qemu/%.gz: third_party/qemu/%.gz.sha256 o/tool/sha256sum
	curl -so $@ https://justine.storage.googleapis.com/qemu/$(subst third_party/qemu/,,$@)
	o/tool/sha256sum -c $<

o/third_party/qemu/4/qemu-x86_64: third_party/qemu/4/qemu-x86_64.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-aarch64: third_party/qemu/4/qemu-aarch64.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-aarch64_be: third_party/qemu/4/qemu-aarch64_be.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-arm: third_party/qemu/4/qemu-arm.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-i386: third_party/qemu/4/qemu-i386.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-m68k: third_party/qemu/4/qemu-m68k.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-microblaze: third_party/qemu/4/qemu-microblaze.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-riscv64: third_party/qemu/4/qemu-riscv64.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-mips: third_party/qemu/4/qemu-mips.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-mipsel: third_party/qemu/4/qemu-mipsel.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-mips64: third_party/qemu/4/qemu-mips64.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-mips64el: third_party/qemu/4/qemu-mips64el.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-ppc: third_party/qemu/4/qemu-ppc.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-ppc64: third_party/qemu/4/qemu-ppc64.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-ppc64le: third_party/qemu/4/qemu-ppc64le.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-s390x: third_party/qemu/4/qemu-s390x.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-alpha: third_party/qemu/4/qemu-alpha.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-armeb: third_party/qemu/4/qemu-armeb.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-cris: third_party/qemu/4/qemu-cris.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-hexagon: third_party/qemu/4/qemu-hexagon.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-hppa: third_party/qemu/4/qemu-hppa.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-loongarch64: third_party/qemu/4/qemu-loongarch64.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-microblazeel: third_party/qemu/4/qemu-microblazeel.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-mipsn32: third_party/qemu/4/qemu-mipsn32.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-mipsn32el: third_party/qemu/4/qemu-mipsn32el.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-nios2: third_party/qemu/4/qemu-nios2.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-or1k: third_party/qemu/4/qemu-or1k.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-riscv32: third_party/qemu/4/qemu-riscv32.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-sh4: third_party/qemu/4/qemu-sh4.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-sh4eb: third_party/qemu/4/qemu-sh4eb.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-sparc: third_party/qemu/4/qemu-sparc.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-sparc32plus: third_party/qemu/4/qemu-sparc32plus.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-sparc64: third_party/qemu/4/qemu-sparc64.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-xtensa: third_party/qemu/4/qemu-xtensa.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@

o/third_party/qemu/4/qemu-xtensaeb: third_party/qemu/4/qemu-xtensaeb.gz
	@mkdir -p $(@D)
	gzip -dc <$< >$@
	chmod +x $@
