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

o/$(MODE)/third_party/libc-test/bin/%.elf.ok:							\
		third_party/libc-test/bin/%.elf							\
		o/lib/ld-musl-x86_64.so.1							\
		o/$(MODE)/blink/blink								\
		o/proc/cpuinfo									\
		o/proc/meminfo
	@mkdir -p $(@D)
	o/$(MODE)/blink/blink $<
	@touch $@

o/proc/%: third_party/libc-test/%
	@mkdir -p $(@D)
	cp -f $< $@
	chmod +x $@

LIBC_TEST_TESTS =										\
	o/$(MODE)/third_party/libc-test/bin/2/functional/pthread_cancel.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/strtol.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/crypt.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/string_strstr.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/string_memcpy.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/clock_gettime.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/iconv_open.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/fdopen.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/argv.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/memstream.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/fcntl.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/random.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/mbc.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/search_insque.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/tgmath.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/pthread_tsd.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/socket.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/search_tsearch.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/udiv.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/string.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/basename.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/string_memmem.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/tls_local_exec.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/strftime.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/pthread_cond.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/string_memset.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/wcsstr.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/clocale_mbfuncs.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/setjmp.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/utime.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/env.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/sem_init.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/dirname.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/ungetc.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/wcstol.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/search_lsearch.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/strtold.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/fnmatch.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/fwscanf.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/qsort.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/string_strchr.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/time.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/search_hsearch.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/fscanf.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/functional/sscanf_long.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/functional/string_strcspn.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/printf-fmt-n.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/putenv-doublefree.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/memmem-oob.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/regex-ere-backref.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/iconv-roundtrips.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/malloc-0.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/regex-backref-0.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/regex-negated-range.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/sigreturn.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/rewind-clear-error.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/sigaltstack.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/pthread_condattr_setclock.elf.ok	\
	o/$(MODE)/third_party/libc-test/bin/2/regression/fgets-eof.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/setvbuf-unget.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/ftello-unflushed-append.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/syscall-sign-extend.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/fflush-exit.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/lrand48-signextend.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/pthread_once-deadlock.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/pthread_cond-smasher.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/printf-fmt-g-round.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/pthread_exit-cancel.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/pthread_rwlock-ebusy.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/printf-fmt-g-zeros.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/wcsncpy-read-overflow.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/mbsrtowcs-overflow.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/sscanf-eof.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/lseek-large.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/dn_expand-empty.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/inet_pton-empty-last-field.elf.ok	\
	o/$(MODE)/third_party/libc-test/bin/2/regression/strverscmp.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/wcsstr-false-negative.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/pthread_exit-dtor.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/memmem-oob-read.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/regex-bracket-icase.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/iswspace-null.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/regexec-nosub.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/mkdtemp-failure.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/dn_expand-ptr-0.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/getpwnam_r-errno.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/fgetwc-buffering.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/pthread-robust-detach.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/scanf-nullbyte-char.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/flockfile-list.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/inet_ntop-v4mapped.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/regex-escaped-high-byte.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/mkstemp-failure.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/uselocale-0.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/sigprocmask-internal.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/statvfs.elf.ok				\
	o/$(MODE)/third_party/libc-test/bin/2/regression/scanf-match-literal-eof.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/getpwnam_r-crash.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/regression/scanf-bytes-consumed.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/3/regression/pthread_create-oom.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/3/regression/daemon-failure.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/3/functional/inet_pton.elf.ok

ifneq ($(wildcard /dev/shm),)
ifneq ($(HOST_SYSTEM), FreeBSD)
LIBC_TEST_TESTS +=										\
	o/$(MODE)/third_party/libc-test/bin/2/functional/sem_open.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/sem_close-unmap.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/2/regression/pthread_cancel-sem_wait.elf.ok		\
	o/$(MODE)/third_party/libc-test/bin/2/functional/pthread_cancel-points.elf.ok
endif
endif

# When creating files, Linux sets st_gid to getegid() but Darwin and
# FreeBSD appear to always sets it to 0 (wheel). This test also just
# seems to be flaky in general, printing errors like "st.st_atime<=t
# failed: 1675987682 > 1675970318".
#
# ifneq ($(HOST_SYSTEM), Darwin)
# ifneq ($(HOST_SYSTEM), FreeBSD)
# LIBC_TEST_TESTS += o/$(MODE)/third_party/libc-test/bin/2/functional/stat.elf.ok
# endif
# endif

# No RLIMIT_NPROC on Cygwin
ifneq ($(HOST_OS), Cygwin)
LIBC_TEST_TESTS += o/$(MODE)/third_party/libc-test/bin/2/regression/pthread_atfork-errno-clobber.elf.ok
endif

ifneq ($(HOST_SYSTEM), OpenBSD)
LIBC_TEST_TESTS +=										\
	o/$(MODE)/third_party/libc-test/bin/3/regression/malloc-oom.elf.ok			\
	o/$(MODE)/third_party/libc-test/bin/3/regression/setenv-oom.elf.ok
endif

################################################################################
# FLAKY TESTS

# This test execve()'s the system /bin/sh. GitHub Actions Linux
# environment reports "sh: error while loading shared libraries:
# libc.so.6: cannot stat shared object: No such file or directory".
#
# o/$(MODE)/third_party/libc-test/bin/2/regression/execle-env.elf.ok

# This test also executes /bin/sh so it's failing on GitHub Actions. It
# also fails for unexplained reasons on OpenBSD.
#
# o/$(MODE)/third_party/libc-test/bin/2/functional/vfork.elf.ok

# These tests work fine but they launch /bin/sh so avoiding adding them
# to `make check` until we find some way to detect GA environment.
#
# o/$(MODE)/third_party/libc-test/bin/3/functional/popen.elf.ok
# o/$(MODE)/third_party/libc-test/bin/3/functional/spawn.elf.ok

# Fails on GitHub Actions Linux with "SEGMENTATION FAULT AT ADDRESS 10"
# and this hasn't happened in any other environment.
# https://github.com/jart/blink/actions/runs/4131367559/jobs/7138932392
#
# o/$(MODE)/third_party/libc-test/bin/2/functional/tls_init.elf.ok

################################################################################

o/$(MODE)/third_party/libc-test: $(LIBC_TEST_TESTS)
	@mkdir -p $(@D)
	@touch $@
