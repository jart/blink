#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi ──────────────────────┘

o/$(MODE)/tool/config/%.com: tool/config/%.c
	@mkdir -p $(@D)
	$(CC) -o $@ $< $(LDLIBS)

o/$(MODE)/tool/config/zlib.com: private LDLIBS += -lz
o/$(MODE)/tool/config/libunwind.com: private LDLIBS += -lunwind -lucontext -llzma
