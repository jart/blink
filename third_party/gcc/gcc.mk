#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

third_party/gcc/%.xz: third_party/gcc/%.xz.sha256
	curl -so $@ https://justine.lol/compilers/$(notdir $@)
	build/bootstrap/sha256sum.com $<

o/$(MODE)/i486/%.o: %.c o/third_party/gcc/i486/bin/i486-linux-musl-gcc
	@mkdir -p $(@D)
	o/third_party/gcc/i486/bin/i486-linux-musl-gcc -static -Werror $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/$(MODE)/m68k/%.o: %.c o/third_party/gcc/m68k/bin/m68k-linux-musl-gcc
	@mkdir -p $(@D)
	o/third_party/gcc/m68k/bin/m68k-linux-musl-gcc -static -Werror $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/$(MODE)/x86_64/%.o: %.c o/third_party/gcc/x86_64/bin/x86_64-linux-musl-gcc
	@mkdir -p $(@D)
	o/third_party/gcc/x86_64/bin/x86_64-linux-musl-gcc -static -Werror $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/$(MODE)/x86_64-gcc48/%.o: %.c o/third_party/gcc/x86_64-gcc48/bin/x86_64-linux-musl-gcc
	@mkdir -p $(@D)
	o/third_party/gcc/x86_64-gcc48/bin/x86_64-linux-musl-gcc -static -Werror $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/$(MODE)/arm/%.o: %.c o/third_party/gcc/arm/bin/arm-linux-musleabi-gcc
	@mkdir -p $(@D)
	o/third_party/gcc/arm/bin/arm-linux-musleabi-gcc -static -Werror $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/$(MODE)/aarch64/%.o: %.c o/third_party/gcc/aarch64/bin/aarch64-linux-musl-gcc
	@mkdir -p $(@D)
	o/third_party/gcc/aarch64/bin/aarch64-linux-musl-gcc -static -Werror $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/$(MODE)/riscv64/%.o: %.c o/third_party/gcc/riscv64/bin/riscv64-linux-musl-gcc
	@mkdir -p $(@D)
	o/third_party/gcc/riscv64/bin/riscv64-linux-musl-gcc -static -Werror $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/$(MODE)/mips/%.o: %.c o/third_party/gcc/mips/bin/mips-linux-musl-gcc
	@mkdir -p $(@D)
	o/third_party/gcc/mips/bin/mips-linux-musl-gcc -static -Werror $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/$(MODE)/mipsel/%.o: %.c o/third_party/gcc/mipsel/bin/mipsel-linux-musl-gcc
	@mkdir -p $(@D)
	o/third_party/gcc/mipsel/bin/mipsel-linux-musl-gcc -static -Werror $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/$(MODE)/mips64/%.o: %.c o/third_party/gcc/mips64/bin/mips64-linux-musl-gcc
	@mkdir -p $(@D)
	o/third_party/gcc/mips64/bin/mips64-linux-musl-gcc -static -Werror $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/$(MODE)/mips64el/%.o: %.c o/third_party/gcc/mips64el/bin/mips64el-linux-musl-gcc
	@mkdir -p $(@D)
	o/third_party/gcc/mips64el/bin/mips64el-linux-musl-gcc -static -Werror $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/$(MODE)/s390x/%.o: %.c o/third_party/gcc/s390x/bin/s390x-linux-musl-gcc
	@mkdir -p $(@D)
	o/third_party/gcc/s390x/bin/s390x-linux-musl-gcc -static -Werror $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/$(MODE)/microblaze/%.o: %.c o/third_party/gcc/microblaze/bin/microblaze-linux-musl-gcc
	@mkdir -p $(@D)
	o/third_party/gcc/microblaze/bin/microblaze-linux-musl-gcc -static -Werror $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/$(MODE)/powerpc/%.o: %.c o/third_party/gcc/powerpc/bin/powerpc-linux-musl-gcc
	@mkdir -p $(@D)
	o/third_party/gcc/powerpc/bin/powerpc-linux-musl-gcc -static -Werror $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/$(MODE)/powerpc64le/%.o: %.c o/third_party/gcc/powerpc64le/bin/powerpc64le-linux-musl-gcc
	@mkdir -p $(@D)
	o/third_party/gcc/powerpc64le/bin/powerpc64le-linux-musl-gcc -static -Werror $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c -o $@ $<

o/third_party/gcc/i486/bin/i486-linux-musl-gcc:			\
		third_party/gcc/x86_64-linux-musl__i486-linux-musl__g++-7.2.0.tar.xz
	mkdir -p o/third_party/gcc/i486
	tar -C o/third_party/gcc/i486 -xJf $<
	touch $@

o/third_party/gcc/m68k/bin/m68k-linux-musl-gcc:			\
		third_party/gcc/x86_64-linux-musl__m68k-linux-musl__gcc-5.3.0.tar.xz
	mkdir -p o/third_party/gcc/m68k
	tar -C o/third_party/gcc/m68k -xJf $<
	touch $@

o/third_party/gcc/x86_64/bin/x86_64-linux-musl-gcc:		\
		third_party/gcc/x86_64-linux-musl__x86_64-linux-musl__g++-7.2.0.tar.xz
	mkdir -p o/third_party/gcc/x86_64
	tar -C o/third_party/gcc/x86_64 -xJf $<
	touch $@

o/third_party/gcc/x86_64-gcc48/bin/x86_64-linux-musl-gcc:	\
		third_party/gcc/x86_64-linux-musl__x86_64-linux-musl__gcc-4.8.5.tar.xz
	mkdir -p o/third_party/gcc/x86_64-gcc48
	tar -C o/third_party/gcc/x86_64-gcc48 -xJf $<
	touch $@

o/third_party/gcc/arm/bin/arm-linux-musleabi-gcc:		\
		third_party/gcc/x86_64-linux-musl__arm-linux-musleabi__g++-7.2.0.tar.xz
	mkdir -p o/third_party/gcc/arm
	tar -C o/third_party/gcc/arm -xJf $<
	touch $@

o/third_party/gcc/aarch64/bin/aarch64-linux-musl-gcc:		\
		third_party/gcc/x86_64-linux-musl__aarch64-linux-musl__g++-7.2.0.tar.xz
	mkdir -p o/third_party/gcc/aarch64
	tar -C o/third_party/gcc/aarch64 -xJf $<
	touch $@

o/third_party/gcc/riscv64/bin/riscv64-linux-musl-gcc:		\
		third_party/gcc/x86_64-linux-musl__riscv64-linux-musl__gcc-9.2.0.tar.xz
	mkdir -p o/third_party/gcc/riscv64
	tar -C o/third_party/gcc/riscv64 -xJf $<
	touch $@

o/third_party/gcc/mips/bin/mips-linux-musl-gcc:			\
		third_party/gcc/x86_64-linux-musl__mips-linux-musl__g++-7.2.0.tar.xz
	mkdir -p o/third_party/gcc/mips
	tar -C o/third_party/gcc/mips -xJf $<
	touch $@

o/third_party/gcc/mipsel/bin/mipsel-linux-musl-gcc:		\
		third_party/gcc/x86_64-linux-musl__mipsel-linux-musl__g++-9.2.0.tar.xz
	mkdir -p o/third_party/gcc/mipsel
	tar -C o/third_party/gcc/mipsel -xJf $<
	touch $@

o/third_party/gcc/mips64/bin/mips64-linux-musl-gcc:		\
		third_party/gcc/x86_64-linux-musl__mips64-linux-musl__g++-7.2.0.tar.xz
	mkdir -p o/third_party/gcc/mips64
	tar -C o/third_party/gcc/mips64 -xJf $<
	touch $@

o/third_party/gcc/mips64el/bin/mips64el-linux-musl-gcc:		\
		third_party/gcc/x86_64-linux-musl__mips64el-linux-musl__gcc-5.3.0.tar.xz
	mkdir -p o/third_party/gcc/mips64el
	tar -C o/third_party/gcc/mips64el -xJf $<
	touch $@

o/third_party/gcc/s390x/bin/s390x-linux-musl-gcc:		\
		third_party/gcc/x86_64-linux-musl__s390x-linux-musl__g++-7.2.0.tar.xz
	mkdir -p o/third_party/gcc/s390x
	tar -C o/third_party/gcc/s390x -xJf $<
	touch $@

o/third_party/gcc/microblaze/bin/microblaze-linux-musl-gcc:	\
		third_party/gcc/x86_64-linux-musl__microblaze-linux-musl__g++-7.2.0.tar.xz
	mkdir -p o/third_party/gcc/microblaze
	tar -C o/third_party/gcc/microblaze -xJf $<
	touch $@

o/third_party/gcc/powerpc/bin/powerpc-linux-musl-gcc:		\
		third_party/gcc/x86_64-linux-musl__powerpc-linux-musl__g++-7.2.0.tar.xz
	mkdir -p o/third_party/gcc/powerpc
	tar -C o/third_party/gcc/powerpc -xJf $<
	touch $@

o/third_party/gcc/powerpc64le/bin/powerpc64le-linux-musl-gcc:	\
		third_party/gcc/x86_64-linux-musl__powerpc64le-linux-musl__g++-7.2.0.tar.xz
	mkdir -p o/third_party/gcc/powerpc64le
	tar -C o/third_party/gcc/powerpc64le -xJf $<
	touch $@
