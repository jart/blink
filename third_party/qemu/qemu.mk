#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

third_party/qemu/%.xz: third_party/qemu/%.xz.sha256 $(VM)
	curl -so $@ https://justine.lol/compilers/$(notdir $@)
	$(VM) build/bootstrap/sha256sum.com $<

o/third_party/qemu/qemu-aarch64: third_party/qemu/qemu-aarch64-static.xz $(VM)
	@mkdir -p $(@D)
	xz -dc <$< >$@
	chmod +x $@

o/third_party/qemu/qemu-arm: third_party/qemu/qemu-arm-static.xz $(VM)
	@mkdir -p $(@D)
	xz -dc <$< >$@
	chmod +x $@

o/third_party/qemu/qemu-i486: third_party/qemu/qemu-i486-static.xz $(VM)
	@mkdir -p $(@D)
	xz -dc <$< >$@
	chmod +x $@

o/third_party/qemu/qemu-m68k: third_party/qemu/qemu-m68k-static.xz $(VM)
	@mkdir -p $(@D)
	xz -dc <$< >$@
	chmod +x $@

o/third_party/qemu/qemu-microblaze: third_party/qemu/qemu-microblaze-static.xz $(VM)
	@mkdir -p $(@D)
	xz -dc <$< >$@
	chmod +x $@

o/third_party/qemu/qemu-riscv64: third_party/qemu/qemu-riscv64-static.xz $(VM)
	@mkdir -p $(@D)
	xz -dc <$< >$@
	chmod +x $@

o/third_party/qemu/qemu-mips: third_party/qemu/qemu-mips-static.xz $(VM)
	@mkdir -p $(@D)
	xz -dc <$< >$@
	chmod +x $@

o/third_party/qemu/qemu-mipsel: third_party/qemu/qemu-mipsel-static.xz $(VM)
	@mkdir -p $(@D)
	xz -dc <$< >$@
	chmod +x $@

o/third_party/qemu/qemu-mips64: third_party/qemu/qemu-mips64-static.xz $(VM)
	@mkdir -p $(@D)
	xz -dc <$< >$@
	chmod +x $@

o/third_party/qemu/qemu-mips64el: third_party/qemu/qemu-mips64el-static.xz $(VM)
	@mkdir -p $(@D)
	xz -dc <$< >$@
	chmod +x $@

o/third_party/qemu/qemu-powerpc: third_party/qemu/qemu-powerpc-static.xz $(VM)
	@mkdir -p $(@D)
	xz -dc <$< >$@
	chmod +x $@

o/third_party/qemu/qemu-powerpc64le: third_party/qemu/qemu-powerpc64le-static.xz $(VM)
	@mkdir -p $(@D)
	xz -dc <$< >$@
	chmod +x $@

o/third_party/qemu/qemu-s390x: third_party/qemu/qemu-s390x-static.xz $(VM)
	@mkdir -p $(@D)
	xz -dc <$< >$@
	chmod +x $@

o/third_party/qemu/qemu-x86_64: third_party/qemu/qemu-x86_64-static.xz $(VM)
	@mkdir -p $(@D)
	xz -dc <$< >$@
	chmod +x $@
