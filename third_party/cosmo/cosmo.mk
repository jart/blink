#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

.PRECIOUS: third_party/cosmo/%.gz
third_party/cosmo/%.gz: third_party/cosmo/%.gz.sha256 o/tool/sha256sum
	curl -so $@ https://justine.storage.googleapis.com/cosmotests/$(subst third_party/cosmo/,,$@)
	o/tool/sha256sum -c $<

.PRECIOUS: third_party/cosmo/%.com.dbg
third_party/cosmo/%.com.dbg: third_party/cosmo/%.com.dbg.gz
	gzip -dc <$< >$@
	chmod +x $@

.PRECIOUS: third_party/cosmo/%.com
third_party/cosmo/%.com: third_party/cosmo/%.com.gz third_party/cosmo/%.com.dbg
	gzip -dc <$< >$@
	chmod +x $@

o/$(MODE)/third_party/cosmo/%.com.ok: third_party/cosmo/%.com o/$(MODE)/blink/blink $(VM)
	@mkdir -p $(@D)
	o/$(MODE)/blink/blink $<
	@touch $@

COSMO_TESTS =											\
	o/$(MODE)/third_party/cosmo/8/intrin_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/lockscale_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/palignr_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/pmulhrsw_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/mulaw_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/nanosleep_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/clock_nanosleep_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/palandprintf_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/pshuf_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/popcnt_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/kprintf_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/memmem_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/memcmp_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/memory_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/memrchr_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/parsecidr_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/parsecontentlength_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/parseforwarded_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/parsehoststxt_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/parsehttpdatetime_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/parsehttpmessage_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/parsehttprange_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/parseip_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/parseresolvconf_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/parseurl_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/pascalifydnsname_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/pcmpstr_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/pingpong_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/prototxt_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/rand64_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/qsort_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/regex_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/renameat_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/atoi_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/resolvehostsreverse_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/resolvehoststxt_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/reverse_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/rgb2ansi_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/rngset_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/round_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/roundup2log_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/sad16x8n_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/scalevolume_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/secp384r1_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/putenv_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/note_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/mu_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/servicestxt_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/setitimer_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/setlocale_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/sincos_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/sinh_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/sizetol_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/sleb128_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/snprintf_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/alu_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/bsu_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/divmul_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/test_suite_ecp.com.ok					\
	o/$(MODE)/third_party/cosmo/2/dll_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/asmdown_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/asin_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/atan2_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/argon2_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/counter_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/pthread_detach_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/pthread_mutex_lock_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/pthread_mutex_lock2_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/pthread_spin_lock_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/cescapec_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/clock_gettime_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/cas_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/bilinearscale_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/access_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/a64l_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/_timespec_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/zleb64_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/xslurp_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/chdir_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/mkdir_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/unlinkat_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/makedirs_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/dirstream_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/bitscan_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/commandv_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/closefrom_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/ecvt_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/division_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/test_suite_aes.cbc.com.ok					\
	o/$(MODE)/third_party/cosmo/2/test_suite_cipher.gcm.com.ok				\
	o/$(MODE)/third_party/cosmo/2/test_suite_ctr_drbg.com.ok				\
	o/$(MODE)/third_party/cosmo/2/test_suite_entropy.com.ok					\
	o/$(MODE)/third_party/cosmo/2/test_suite_mpi.com.ok					\
	o/$(MODE)/third_party/cosmo/2/test_suite_md.com.ok					\
	o/$(MODE)/third_party/cosmo/2/crc32_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/crc32c_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/crc32z_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/sigsetjmp_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/escapehtml_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/escapeurlparam_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/escapejsstringliteral_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/erf_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/encodebase64_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/fabs_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/fgets_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/fileexists_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/floor_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/fmemopen_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/fmt_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/fputc_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/gamma_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/tgamma_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/gclongjmp_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/getcwd_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/gz_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/ilogb_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/imaxdiv_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/inv3_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/iso8601_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/itsatrap_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/mu_starvation_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/open_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/stat_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/stackrwx_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/clone_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/cv_test.com.ok						\
	o/$(MODE)/third_party/cosmo/4/writev_test.com.ok					\
	o/$(MODE)/third_party/cosmo/3/sqlite_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/sched_yield_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/pwrite_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/read_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/pread_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/preadv_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/nsync_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/mt19937_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/modrm_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/measureentropy_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/malloc_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/ftell_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/fseeko_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/fputs_test.com.ok						\
	o/$(MODE)/third_party/cosmo/5/pipe_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/arena_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/acos_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/getintegercoefficients8_test.com.ok			\
	o/$(MODE)/third_party/cosmo/2/atan_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/bextra_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/cbrt_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/clock_getres_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/copysign_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/cos_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/cosh_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/complex_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/countbits_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/cv_wait_example_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/expm1_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/fgetln_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/getcontext_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/getenv_test.com.ok					\
	o/$(MODE)/third_party/cosmo/3/ftruncate_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/tmpfile_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/select_test.com.ok					\
	o/$(MODE)/third_party/cosmo/7/utimensat_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/readlinkat_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/signal_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/tkill_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/getdelim_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/getgroups_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/dup_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/sem_timedwait_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/lockipc_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/socket_test.com.ok					\
	o/$(MODE)/third_party/cosmo/5/unix_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/daemon_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/execve_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/sigpending_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/fork_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/setsockopt_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/sigsuspend_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/lock2_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/lock_test.com.ok						\
	o/$(MODE)/third_party/cosmo/5/sigaction_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/pthread_create_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/pthread_exit_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/readansi_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/sendrecvmsg_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/lseek_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/mmap_test.com.ok

# munmap_test triggers SIGSEGV on purpose, making it fail on asan
# TODO(jart): Make this work on asan
ifneq ($(MODE), asan)
COSMO_TESTS += o/$(MODE)/third_party/cosmo/7/munmap_test.com.ok
endif

# write_test is broken on Cygwin due to RLIMIT_FSIZE
# TODO(jart): Why do the other ones flake on Cygwin?
ifneq ($(HOST_OS), Cygwin)
COSMO_TESTS +=											\
	o/$(MODE)/third_party/cosmo/4/dtoa_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/write_test.com.ok						\
	o/$(MODE)/third_party/cosmo/2/once_test.com.ok
endif

o/$(MODE)/third_party/cosmo: $(COSMO_TESTS)
	@mkdir -p $(@D)
	@touch $@

PROBLEMATIC_TESTS =										\
	o/$(MODE)/third_party/cosmo/2/sem_open_test.com.ok					\
	o/$(MODE)/third_party/cosmo/2/sigprocmask_test.com.ok

DARWIN_PROBLEMATIC_TESTS =									\
	o/$(MODE)/third_party/cosmo/2/sched_getaffinity_test.com.ok				\
	o/$(MODE)/third_party/cosmo/2/backtrace_test.com.ok

o/$(MODE)/third_party/cosmo/emulates:								\
		o/$(MODE)/aarch64/third_party/cosmo/8/intrin_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/palandprintf_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/divmul_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/test_suite_ecp.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/lockscale_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/palignr_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/pmulhrsw_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/pshuf_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/alu_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/bsu_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/pthread_mutex_lock2_test.com.emulates	\
		o/$(MODE)/aarch64/third_party/cosmo/2/pthread_mutex_lock_test.com.emulates	\
		o/$(MODE)/aarch64/third_party/cosmo/2/pthread_spin_lock_test.com.emulates	\
		o/$(MODE)/aarch64/third_party/cosmo/2/sincos_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/round_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/kprintf_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/snprintf_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/once_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/mu_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/note_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/counter_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/dll_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/secp384r1_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/parsehttpmessage_test.com.emulates	\
		o/$(MODE)/aarch64/third_party/cosmo/2/parseurl_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/parseip_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/parsehttprange_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/pcmpstr_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/rand64_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/cescapec_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/clock_gettime_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/cas_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/bilinearscale_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/access_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/a64l_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/_timespec_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/zleb64_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/xslurp_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/chdir_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/mkdir_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/unlinkat_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/makedirs_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/dirstream_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/bitscan_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/commandv_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/closefrom_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/ecvt_test.com.emulates			\
		o/$(MODE)/aarch64/third_party/cosmo/2/division_test.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/test_suite_aes.cbc.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/test_suite_cipher.gcm.com.emulates	\
		o/$(MODE)/aarch64/third_party/cosmo/2/test_suite_ctr_drbg.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/test_suite_entropy.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/test_suite_md.com.emulates		\
		o/$(MODE)/aarch64/third_party/cosmo/2/execve_test.com.emulates			\
		o/$(MODE)/mips64el/third_party/cosmo/8/intrin_test.com.emulates			\
		o/$(MODE)/mips64el/third_party/cosmo/2/palandprintf_test.com.emulates		\
		o/$(MODE)/mips64el/third_party/cosmo/2/divmul_test.com.emulates			\
		o/$(MODE)/mips64el/third_party/cosmo/2/lockscale_test.com.emulates		\
		o/$(MODE)/mips64el/third_party/cosmo/2/palignr_test.com.emulates		\
		o/$(MODE)/mips64el/third_party/cosmo/2/pmulhrsw_test.com.emulates		\
		o/$(MODE)/mips64el/third_party/cosmo/2/pshuf_test.com.emulates			\
		o/$(MODE)/mips64el/third_party/cosmo/2/pthread_mutex_lock2_test.com.emulates	\
		o/$(MODE)/mips64el/third_party/cosmo/2/pthread_mutex_lock_test.com.emulates	\
		o/$(MODE)/mips64el/third_party/cosmo/2/pthread_spin_lock_test.com.emulates	\
		o/$(MODE)/s390x/third_party/cosmo/8/intrin_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/palandprintf_test.com.emulates		\
		o/$(MODE)/s390x/third_party/cosmo/2/divmul_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/lockscale_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/palignr_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/pmulhrsw_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/pshuf_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/pthread_mutex_lock2_test.com.emulates	\
		o/$(MODE)/s390x/third_party/cosmo/2/pthread_mutex_lock_test.com.emulates	\
		o/$(MODE)/s390x/third_party/cosmo/2/pthread_spin_lock_test.com.emulates		\
		o/$(MODE)/s390x/third_party/cosmo/2/sincos_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/round_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/kprintf_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/snprintf_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/once_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/note_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/counter_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/dll_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/secp384r1_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/parsehttpmessage_test.com.emulates		\
		o/$(MODE)/s390x/third_party/cosmo/2/parseurl_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/parseip_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/parsehttprange_test.com.emulates		\
		o/$(MODE)/s390x/third_party/cosmo/2/pcmpstr_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/rand64_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/cescapec_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/clock_gettime_test.com.emulates		\
		o/$(MODE)/s390x/third_party/cosmo/2/cas_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/bilinearscale_test.com.emulates		\
		o/$(MODE)/s390x/third_party/cosmo/2/access_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/a64l_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/_timespec_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/zleb64_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/xslurp_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/chdir_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/mkdir_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/unlinkat_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/makedirs_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/dirstream_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/bitscan_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/commandv_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/closefrom_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/ecvt_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/division_test.com.emulates			\
		o/$(MODE)/s390x/third_party/cosmo/2/test_suite_aes.cbc.com.emulates		\
		o/$(MODE)/s390x/third_party/cosmo/2/test_suite_cipher.gcm.com.emulates		\
		o/$(MODE)/s390x/third_party/cosmo/2/test_suite_ctr_drbg.com.emulates		\
		o/$(MODE)/s390x/third_party/cosmo/2/test_suite_entropy.com.emulates		\
		o/$(MODE)/s390x/third_party/cosmo/2/test_suite_md.com.emulates			\
		o/$(MODE)/powerpc64le/third_party/cosmo/8/intrin_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/palandprintf_test.com.emulates	\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/divmul_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/lockscale_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/palignr_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/pmulhrsw_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/pshuf_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/pthread_mutex_lock2_test.com.emulates	\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/pthread_mutex_lock_test.com.emulates	\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/pthread_spin_lock_test.com.emulates	\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/sincos_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/round_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/kprintf_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/snprintf_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/once_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/note_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/counter_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/dll_test.com.emulates			\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/secp384r1_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/parsehttpmessage_test.com.emulates	\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/parseurl_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/parseip_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/parsehttprange_test.com.emulates	\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/pcmpstr_test.com.emulates		\
		o/$(MODE)/powerpc64le/third_party/cosmo/2/rand64_test.com.emulates
	@mkdir -p $(@D)
	@touch $@
