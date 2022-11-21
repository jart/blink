#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

.PRECIOUS: third_party/cosmo/%.gz
third_party/cosmo/%.gz: third_party/cosmo/%.gz.sha256
	curl -so $@ https://justine.lol/cosmotests/$(notdir $@)
	$(VM) build/bootstrap/sha256sum.com $<

.PRECIOUS: third_party/cosmo/%.com.dbg
third_party/cosmo/%.com.dbg: third_party/cosmo/%.com.dbg.gz
	gzip -dc <$< >$@
	chmod +x $@

.PRECIOUS: third_party/cosmo/%.com
third_party/cosmo/%.com: third_party/cosmo/%.com.gz third_party/cosmo/%.com.dbg
	gzip -dc <$< >$@
	chmod +x $@

o/$(MODE)/third_party/cosmo/%.com.ok: third_party/cosmo/%.com o/$(MODE)/blink/blink
	o/$(MODE)/blink/blink $<
	@touch $@

o/$(MODE)/third_party/cosmo:									\
		o/$(MODE)/third_party/cosmo/intrin_test.com.ok					\
		o/$(MODE)/third_party/cosmo/lockscale_test.com.ok				\
		o/$(MODE)/third_party/cosmo/palignr_test.com.ok					\
		o/$(MODE)/third_party/cosmo/pmulhrsw_test.com.ok				\
		o/$(MODE)/third_party/cosmo/mulaw_test.com.ok					\
		o/$(MODE)/third_party/cosmo/nanosleep_test.com.ok				\
		o/$(MODE)/third_party/cosmo/palandprintf_test.com.ok				\
		o/$(MODE)/third_party/cosmo/pshuf_test.com.ok					\
		o/$(MODE)/third_party/cosmo/popcnt_test.com.ok					\
		o/$(MODE)/third_party/cosmo/kprintf_test.com.ok					\
		o/$(MODE)/third_party/cosmo/rand64_test.com.ok					\
		o/$(MODE)/third_party/cosmo/alu_test.com.ok					\
		o/$(MODE)/third_party/cosmo/bsu_test.com.ok					\
		o/$(MODE)/third_party/cosmo/once_test.com.ok					\
		o/$(MODE)/third_party/cosmo/dll_test.com.ok					\
		o/$(MODE)/third_party/cosmo/note_test.com.ok					\
		o/$(MODE)/third_party/cosmo/wait_test.com.ok					\
		o/$(MODE)/third_party/cosmo/counter_test.com.ok					\
		o/$(MODE)/third_party/cosmo/pthread_mutex_lock2_test.com.ok			\
		o/$(MODE)/third_party/cosmo/pthread_mutex_lock_test.com.ok			\
		o/$(MODE)/third_party/cosmo/pthread_spin_lock_test.com.ok
	@mkdir -p $(@D)
	@touch $@

o/$(MODE)/third_party/cosmo/emulates:								\
		o/$(MODE)/aarch64/third_party/cosmo/intrin_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/lockscale_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/palignr_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/pmulhrsw_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/pshuf_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/alu_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/bsu_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/pthread_mutex_lock2_test.com.emulates	\
		o/$(MODE)/aarch64/third_party/cosmo/pthread_mutex_lock_test.com.emulates	\
		o/$(MODE)/aarch64/third_party/cosmo/pthread_spin_lock_test.com.emulates		\
		o/$(MODE)/mips64/third_party/cosmo/intrin_test.com.emulates			\
		o/$(MODE)/mips64/third_party/cosmo/lockscale_test.com.emulates			\
		o/$(MODE)/mips64/third_party/cosmo/palignr_test.com.emulates			\
		o/$(MODE)/mips64/third_party/cosmo/pmulhrsw_test.com.emulates			\
		o/$(MODE)/mips64/third_party/cosmo/pshuf_test.com.emulates			\
		o/$(MODE)/mips64/third_party/cosmo/pthread_mutex_lock2_test.com.emulates	\
		o/$(MODE)/mips64/third_party/cosmo/pthread_mutex_lock_test.com.emulates		\
		o/$(MODE)/mips64/third_party/cosmo/pthread_spin_lock_test.com.emulates		\
		o/$(MODE)/mips64el/third_party/cosmo/intrin_test.com.emulates			\
		o/$(MODE)/mips64el/third_party/cosmo/lockscale_test.com.emulates		\
		o/$(MODE)/mips64el/third_party/cosmo/palignr_test.com.emulates			\
		o/$(MODE)/mips64el/third_party/cosmo/pmulhrsw_test.com.emulates			\
		o/$(MODE)/mips64el/third_party/cosmo/pshuf_test.com.emulates			\
		o/$(MODE)/mips64el/third_party/cosmo/pthread_mutex_lock2_test.com.emulates	\
		o/$(MODE)/mips64el/third_party/cosmo/pthread_mutex_lock_test.com.emulates	\
		o/$(MODE)/mips64el/third_party/cosmo/pthread_spin_lock_test.com.emulates	\
		o/$(MODE)/s390x/third_party/cosmo/intrin_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/lockscale_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/palignr_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/pmulhrsw_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/pshuf_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/alu_test.com.emulates				\
		o/$(MODE)/s390x/third_party/cosmo/pthread_mutex_lock2_test.com.emulates		\
		o/$(MODE)/s390x/third_party/cosmo/pthread_mutex_lock_test.com.emulates		\
		o/$(MODE)/s390x/third_party/cosmo/pthread_spin_lock_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/intrin_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/lockscale_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/palignr_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/pmulhrsw_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/pshuf_test.com.emulates			\
		o/$(MODE)/powerpc64le/third_party/cosmo/pthread_mutex_lock2_test.com.emulates	\
		o/$(MODE)/powerpc64le/third_party/cosmo/pthread_mutex_lock_test.com.emulates	\
		o/$(MODE)/powerpc64le/third_party/cosmo/pthread_spin_lock_test.com.emulates
	@mkdir -p $(@D)
	@touch $@
