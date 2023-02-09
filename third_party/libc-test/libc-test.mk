#-*-mode:makefile-gmake;indent-tabs-mode:t;tab-width:8;coding:utf-8-*-┐
#───vi: set et ft=make ts=8 tw=8 fenc=utf-8 :vi───────────────────────┘

.PRECIOUS: third_party/libc-test/%.gz
third_party/libc-test/%.gz: third_party/libc-test/%.gz.sha256 o/tool/sha256sum
	curl -so $@ https://justine.storage.googleapis.com/libc-test/$(subst third_party/libc-test/,,$@)
	o/tool/sha256sum -c $<

.PRECIOUS: third_party/libc-test/bin/%.elf
third_party/libc-test/bin/%.elf: third_party/libc-test/bin/%.elf.gz
	gzip -dc <$< >$@
	chmod +x $@

o/$(MODE)/third_party/libc-test/bin/%.elf.ok:						\
		third_party/libc-test/bin/%.elf						\
		o/lib/ld-musl-x86_64.so.1						\
		o/$(MODE)/blink/blink							\
		o/proc/cpuinfo								\
		o/proc/meminfo
	@mkdir -p $(@D)
	o/$(MODE)/blink/blink $<
	@touch $@

o/proc/%: third_party/libc-test/%
	@mkdir -p $(@D)
	cp -f $< $@
	chmod +x $@

LIBC_TEST_TESTS =									\
	o//third_party/libc-test/bin/2/functional/pthread_cancel.elf.ok			\
	o//third_party/libc-test/bin/2/functional/strtol.elf.ok				\
	o//third_party/libc-test/bin/2/functional/crypt.elf.ok				\
	o//third_party/libc-test/bin/2/functional/string_strstr.elf.ok			\
	o//third_party/libc-test/bin/2/functional/tls_init.elf.ok			\
	o//third_party/libc-test/bin/2/functional/string_memcpy.elf.ok			\
	o//third_party/libc-test/bin/2/functional/clock_gettime.elf.ok			\
	o//third_party/libc-test/bin/2/functional/iconv_open.elf.ok			\
	o//third_party/libc-test/bin/2/functional/fdopen.elf.ok				\
	o//third_party/libc-test/bin/2/functional/argv.elf.ok				\
	o//third_party/libc-test/bin/2/functional/memstream.elf.ok			\
	o//third_party/libc-test/bin/2/functional/fcntl.elf.ok				\
	o//third_party/libc-test/bin/2/functional/random.elf.ok				\
	o//third_party/libc-test/bin/2/functional/mbc.elf.ok				\
	o//third_party/libc-test/bin/2/functional/search_insque.elf.ok			\
	o//third_party/libc-test/bin/2/functional/tgmath.elf.ok				\
	o//third_party/libc-test/bin/2/functional/pthread_tsd.elf.ok			\
	o//third_party/libc-test/bin/2/functional/socket.elf.ok				\
	o//third_party/libc-test/bin/2/functional/search_tsearch.elf.ok			\
	o//third_party/libc-test/bin/2/functional/udiv.elf.ok				\
	o//third_party/libc-test/bin/2/functional/string.elf.ok				\
	o//third_party/libc-test/bin/2/functional/basename.elf.ok			\
	o//third_party/libc-test/bin/2/functional/string_memmem.elf.ok			\
	o//third_party/libc-test/bin/2/functional/tls_local_exec.elf.ok			\
	o//third_party/libc-test/bin/2/functional/strftime.elf.ok			\
	o//third_party/libc-test/bin/2/functional/pthread_cond.elf.ok			\
	o//third_party/libc-test/bin/2/functional/string_memset.elf.ok			\
	o//third_party/libc-test/bin/2/functional/wcsstr.elf.ok				\
	o//third_party/libc-test/bin/2/functional/clocale_mbfuncs.elf.ok		\
	o//third_party/libc-test/bin/2/functional/setjmp.elf.ok				\
	o//third_party/libc-test/bin/2/functional/utime.elf.ok				\
	o//third_party/libc-test/bin/2/functional/env.elf.ok				\
	o//third_party/libc-test/bin/2/functional/sem_init.elf.ok			\
	o//third_party/libc-test/bin/2/functional/dirname.elf.ok			\
	o//third_party/libc-test/bin/2/functional/ungetc.elf.ok				\
	o//third_party/libc-test/bin/2/functional/wcstol.elf.ok				\
	o//third_party/libc-test/bin/2/functional/search_lsearch.elf.ok			\
	o//third_party/libc-test/bin/2/functional/strtold.elf.ok			\
	o//third_party/libc-test/bin/2/functional/fnmatch.elf.ok			\
	o//third_party/libc-test/bin/2/functional/fwscanf.elf.ok			\
	o//third_party/libc-test/bin/2/functional/qsort.elf.ok				\
	o//third_party/libc-test/bin/2/functional/string_strchr.elf.ok			\
	o//third_party/libc-test/bin/2/functional/time.elf.ok				\
	o//third_party/libc-test/bin/2/functional/search_hsearch.elf.ok			\
	o//third_party/libc-test/bin/2/functional/fscanf.elf.ok				\
	o//third_party/libc-test/bin/2/functional/sscanf_long.elf.ok			\
	o//third_party/libc-test/bin/2/functional/string_strcspn.elf.ok			\
	o//third_party/libc-test/bin/2/regression/printf-fmt-n.elf.ok			\
	o//third_party/libc-test/bin/2/regression/putenv-doublefree.elf.ok		\
	o//third_party/libc-test/bin/2/regression/memmem-oob.elf.ok			\
	o//third_party/libc-test/bin/2/regression/regex-ere-backref.elf.ok		\
	o//third_party/libc-test/bin/2/regression/iconv-roundtrips.elf.ok		\
	o//third_party/libc-test/bin/2/regression/malloc-0.elf.ok			\
	o//third_party/libc-test/bin/2/regression/regex-backref-0.elf.ok		\
	o//third_party/libc-test/bin/2/regression/regex-negated-range.elf.ok		\
	o//third_party/libc-test/bin/2/regression/sigreturn.elf.ok			\
	o//third_party/libc-test/bin/2/regression/rewind-clear-error.elf.ok		\
	o//third_party/libc-test/bin/2/regression/sigaltstack.elf.ok			\
	o//third_party/libc-test/bin/2/regression/pthread_condattr_setclock.elf.ok	\
	o//third_party/libc-test/bin/2/regression/fgets-eof.elf.ok			\
	o//third_party/libc-test/bin/2/regression/setvbuf-unget.elf.ok			\
	o//third_party/libc-test/bin/2/regression/ftello-unflushed-append.elf.ok	\
	o//third_party/libc-test/bin/2/regression/syscall-sign-extend.elf.ok		\
	o//third_party/libc-test/bin/2/regression/fflush-exit.elf.ok			\
	o//third_party/libc-test/bin/2/regression/lrand48-signextend.elf.ok		\
	o//third_party/libc-test/bin/2/regression/pthread_once-deadlock.elf.ok		\
	o//third_party/libc-test/bin/2/regression/pthread_cond-smasher.elf.ok		\
	o//third_party/libc-test/bin/2/regression/printf-fmt-g-round.elf.ok		\
	o//third_party/libc-test/bin/2/regression/pthread_exit-cancel.elf.ok		\
	o//third_party/libc-test/bin/2/regression/pthread_rwlock-ebusy.elf.ok		\
	o//third_party/libc-test/bin/2/regression/printf-fmt-g-zeros.elf.ok		\
	o//third_party/libc-test/bin/2/regression/wcsncpy-read-overflow.elf.ok		\
	o//third_party/libc-test/bin/2/regression/mbsrtowcs-overflow.elf.ok		\
	o//third_party/libc-test/bin/2/regression/sscanf-eof.elf.ok			\
	o//third_party/libc-test/bin/2/regression/lseek-large.elf.ok			\
	o//third_party/libc-test/bin/2/regression/dn_expand-empty.elf.ok		\
	o//third_party/libc-test/bin/2/regression/inet_pton-empty-last-field.elf.ok	\
	o//third_party/libc-test/bin/2/regression/strverscmp.elf.ok			\
	o//third_party/libc-test/bin/2/regression/wcsstr-false-negative.elf.ok		\
	o//third_party/libc-test/bin/2/regression/pthread_exit-dtor.elf.ok		\
	o//third_party/libc-test/bin/2/regression/memmem-oob-read.elf.ok		\
	o//third_party/libc-test/bin/2/regression/regex-bracket-icase.elf.ok		\
	o//third_party/libc-test/bin/2/regression/iswspace-null.elf.ok			\
	o//third_party/libc-test/bin/2/regression/regexec-nosub.elf.ok			\
	o//third_party/libc-test/bin/2/regression/mkdtemp-failure.elf.ok		\
	o//third_party/libc-test/bin/2/regression/dn_expand-ptr-0.elf.ok		\
	o//third_party/libc-test/bin/2/regression/getpwnam_r-errno.elf.ok		\
	o//third_party/libc-test/bin/2/regression/fgetwc-buffering.elf.ok		\
	o//third_party/libc-test/bin/2/regression/pthread-robust-detach.elf.ok		\
	o//third_party/libc-test/bin/2/regression/scanf-nullbyte-char.elf.ok		\
	o//third_party/libc-test/bin/2/regression/flockfile-list.elf.ok			\
	o//third_party/libc-test/bin/2/regression/inet_ntop-v4mapped.elf.ok		\
	o//third_party/libc-test/bin/2/regression/regex-escaped-high-byte.elf.ok	\
	o//third_party/libc-test/bin/2/regression/mkstemp-failure.elf.ok		\
	o//third_party/libc-test/bin/2/regression/uselocale-0.elf.ok			\
	o//third_party/libc-test/bin/2/regression/sigprocmask-internal.elf.ok		\
	o//third_party/libc-test/bin/2/regression/statvfs.elf.ok			\
	o//third_party/libc-test/bin/2/regression/scanf-match-literal-eof.elf.ok	\
	o//third_party/libc-test/bin/2/regression/getpwnam_r-crash.elf.ok		\
	o//third_party/libc-test/bin/2/regression/scanf-bytes-consumed.elf.ok		\
	o//third_party/libc-test/bin/3/regression/malloc-oom.elf.ok			\
	o//third_party/libc-test/bin/3/functional/inet_pton.elf.ok

ifeq ($(shell [ -d /dev/shm ] && echo yes), yes)
LIBC_TEST_TESTS +=									\
	o//third_party/libc-test/bin/2/functional/sem_open.elf.ok			\
	o//third_party/libc-test/bin/2/regression/sem_close-unmap.elf.ok		\
	o//third_party/libc-test/bin/2/regression/pthread_cancel-sem_wait.elf.ok	\
	o//third_party/libc-test/bin/2/functional/pthread_cancel-points.elf.ok
endif

# When creating files, Linux sets st_gid to getegid() but Darwin appears
# to always sets it to 0 (wheel).
ifneq ($(HOST_SYSTEM), Darwin)
LIBC_TEST_TESTS += o//third_party/libc-test/bin/2/functional/stat.elf.ok
endif

# No RLIMIT_NPROC on Cygwin
ifneq ($(HOST_OS), Cygwin)
LIBC_TEST_TESTS += o//third_party/libc-test/bin/2/regression/pthread_atfork-errno-clobber.elf.ok
endif

################################################################################
# FLAKY TESTS

# pthread_mutex_unlock() fails on 5% of invocations because the the
# thread set_child_tid mismatches the value stored within the mutex
#
# o//third_party/libc-test/bin/2/functional/pthread_mutex.elf.ok

# This test execve()'s the system /bin/sh. GitHub Actions Linux
# environment reports "sh: error while loading shared libraries:
# libc.so.6: cannot stat shared object: No such file or directory".
#
# o//third_party/libc-test/bin/2/regression/execle-env.elf.ok

# This test also executes /bin/sh so it's failing on GitHub Actions. It
# also fails for unexplained reasons on OpenBSD.
#
# o//third_party/libc-test/bin/2/functional/vfork.elf.ok

################################################################################

o/$(MODE)/third_party/libc-test: $(LIBC_TEST_TESTS)
	@mkdir -p $(@D)
	@touch $@
