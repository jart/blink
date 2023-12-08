#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#── vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi ──────────────────────┘

.PRECIOUS: third_party/musl/%.gz
third_party/musl/%.gz: third_party/musl/%.gz.sha256 o/tool/sha256sum
	curl -so $@ https://justine.storage.googleapis.com/musl/$(subst third_party/musl/,,$@)
	o/tool/sha256sum -c $<

.PRECIOUS: third_party/musl/%
third_party/musl/%: third_party/musl/%.gz
	gzip -dc <$< >$@
	chmod +x $@

o/lib/ld-musl-x86_64.so.1: third_party/musl/lib/1/ld-musl-x86_64.so.1
	@mkdir -p $(@D)
	cp -f $< $@
	chmod +x $@
