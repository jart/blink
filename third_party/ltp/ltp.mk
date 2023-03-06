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

o/$(MODE)/third_party/ltp/bin/%.elf.ok:					\
		third_party/ltp/bin/%.elf				\
		o/lib/ld-musl-x86_64.so.1				\
		o/$(MODE)/blink/blink					\
		o/proc/cpuinfo						\
		o/proc/meminfo
	@mkdir -p $(@D)
	o/$(MODE)/blink/blink $< $(LTP_ARGS)
	@touch $@

o/$(MODE)/third_party/ltp/bin/1/growfiles.elf.ok: private LTP_ARGS = -f "$(TMPDIR)/gf"

.PRECIOUS: o/proc/%
o/proc/%: third_party/ltp/%
	@mkdir -p $(@D)
	cp -f $< $@
	chmod +x $@

LTP_TESTS =								\
	o/$(MODE)/third_party/ltp/bin/2/hackbench.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/accept01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/accept4_01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/alarm03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/alarm05.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/alarm06.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/alarm07.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/atof01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/bind01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/bind03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/brk02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/chown01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/clock_getres01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/clock_nanosleep04.elf.ok	\
	o/$(MODE)/third_party/ltp/bin/1/close01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/close02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/close_range02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/confstr01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/creat01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/creat03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/dup01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/dup02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/dup03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/dup04.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/dup05.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/dup07.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/dup201.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/dup202.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/dup203.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/dup204.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/dup205.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/dup206.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/dup207.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/dup3_01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/dup3_02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/exit01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/exit02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/exit_group01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fchdir01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fchdir02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fchmod01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fchown01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fchownat01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl01_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl02_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl03_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl04.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl04_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl08.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl08_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl09.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl09_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl10.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl10_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl11.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl11_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl14.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl14_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl15.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl15_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl16.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl16_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl17.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl17_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl19.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl19_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl20.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl20_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl21.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl21_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl22.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl22_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl27.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl27_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl28.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl28_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fcntl29.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fcntl29_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fdatasync01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fdatasync02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/flock01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/flock02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/flock03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/flock04.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/flock06.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fork01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fork03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fork04.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fork05.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fork06.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fork07.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fork08.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fork10.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fork11.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fsync03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/ftruncate01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/ftruncate01_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/ftruncate03.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/ftruncate03_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/futex_wait01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/futex_wait04.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/futex_wake01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getcwd02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/getcwd03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/getdents01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/getdents02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getdomainname01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getegid01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getegid02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/geteuid01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/geteuid02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getgid03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/gethostname01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getitimer01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getpagesize01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getpgid01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getpgrp01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getpid02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/getppid02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getpriority02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/getrandom01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getrandom02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getrandom03.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getrlimit02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getrusage01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getsid01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/gettid01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/gettimeofday01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getuid01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/growfiles.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/hackbench.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/in6_01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/inode01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/kill06.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/kill08.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/kill09.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/link02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/link03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/link05.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/listen01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/llseek01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/llseek02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/llseek03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/locktests.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/lseek01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/lseek02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/lseek07.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/mmap01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/mmap02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/mmap03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/mmap04.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/mmap05.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/mmap06.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/mmap07.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/mmap08.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/mmap09.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/mmap19.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/mmapstress04.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/mprotect02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/mprotect03.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/msync01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/msync02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/munmap01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/munmap02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/munmap03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/nanosleep02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fstat02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/open03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/open06.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/open07.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/open09.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pipe01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pipe02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pipe03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pipe04.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pipe06.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pipe07.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pipe08.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pipe09.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pipe10.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pipe11.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pipe13.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/poll01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pread01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pread01_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/pread02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pread02_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/preadv01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/preadv01_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/pselect02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/pselect02_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/pselect03.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/pselect03_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/pwrite01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pwrite01_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/pwrite02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pwrite02_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/pwrite03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pwrite03_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/pwrite04.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pwrite04_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/pwritev01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/pwritev01_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/read01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/read02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/read03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/read04.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/readdir01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/readv01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/recvmsg02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/rmdir01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/sbrk02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/select01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/select04.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/set_robust_list01.elf.ok	\
	o/$(MODE)/third_party/ltp/bin/1/set_tid_address01.elf.ok	\
	o/$(MODE)/third_party/ltp/bin/1/setpgid01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/setpgid02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/setpgrp01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/setpgrp02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/setsid01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/sigaction01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/sigaction02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/rt_sigaction02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/rt_sigaction03.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/sigaltstack01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/sigaltstack02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/signal01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/signal02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/signal04.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/signal06.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/sigprocmask01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/rt_sigprocmask02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/sigsuspend01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/socket01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/socket02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/socketpair01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/socketpair02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/stat02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/stat02_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/symlink01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/symlink02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/symlink04.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/symlink05.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/symlinkat01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/tkill01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/tkill02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/truncate02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/truncate02_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/unlink05.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/wait01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/wait02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/wait401.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/wait402.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/wait403.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/waitpid01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/waitpid02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/waitpid03.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/waitpid04.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/waitpid06.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/waitpid07.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/waitpid08.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/waitpid09.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/waitpid10.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/waitpid11.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/waitpid12.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/waitpid13.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/write01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/write02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/write03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/write04.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/write05.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/write06.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/writetest.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/writev01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/writev02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/writev05.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/writev06.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/writev07.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/sendto01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/setitimer02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/unlinkat01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/futimesat01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/unlink07.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/send01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/3/sendmsg01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/sched_yield01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/readlinkat01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/readlinkat02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/readv02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/realpath01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/openat01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/recvmmsg01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/sendmmsg01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/sendmmsg02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/recvfrom01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/sendfile02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/sendfile03.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/sendfile04.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/sendfile05.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/sendfile06.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/sendfile07.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/sendfile08.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/sendfile09.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/openfile.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/connect01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/chdir04.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/3/epoll-ltp.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_create01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_create02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_create1_01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_create1_02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_ctl01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_ctl02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_ctl03.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_ctl04.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_ctl05.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_pwait01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_pwait02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_pwait04.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_pwait05.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_wait01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_wait03.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_wait04.elf.ok

ifneq ($(HOST_SYSTEM), FreeBSD)
# These tests make the system get wrekt.
LTP_TESTS +=								\
	o/$(MODE)/third_party/ltp/bin/1/pause02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/creat05.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fork09.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/inode02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/open04.elf.ok
endif

ifneq ($(HOST_SYSTEM), FreeBSD)
# Something weird about the sticky bit.
LTP_TESTS += o/$(MODE)/third_party/ltp/bin/1/open01.elf.ok
endif

ifneq ($(HOST_SYSTEM), FreeBSD)
# The FreeBSD manual states "The maximum number of seconds allowed for
# it_interval and it_value in setitimer() is 100000000", but this test
# needs the platform to support values like INT_MAX.
LTP_TESTS += o/$(MODE)/third_party/ltp/bin/1/alarm02.elf.ok
endif

ifneq ($(HOST_SYSTEM), FreeBSD)
# This passes but complains about an EXDEV errno mismatch (possibly with
# procfs) and therefore would be expensive and/or tricky to polyfill.
LTP_TESTS += o/$(MODE)/third_party/ltp/bin/1/linkat01.elf.ok
endif

# These tests won't pass unless Linux-specific rlimits are present,
# RLIMIT_LOCKS, RLIMIT_MSGQUEUE, RLIMIT_RTTIME, etc.
ifneq ($(HOST_SYSTEM), FreeBSD)
LTP_TESTS +=								\
	o/$(MODE)/third_party/ltp/bin/1/getrlimit01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getrlimit03.elf.ok
endif

# These tests require procfs which currently isn't simulated.
ifneq ($(HOST_SYSTEM), FreeBSD)
LTP_TESTS +=								\
	o/$(MODE)/third_party/ltp/bin/1/clock_gettime04.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/futex_wait02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/futex_wait03.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/nanosleep04.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getpgid02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getppid01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getuid03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/getsid02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/getpid01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pause03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pause01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fsync02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/mmap12.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/kill03.elf.ok
endif

ifeq ($(shell id -u), 0)
LTP_TESTS +=								\
	o/$(MODE)/third_party/ltp/bin/1/setegid02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/2/open08.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/open10.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/stat03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/rename09.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/access01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/access02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/access03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/autogroup01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/chmod07.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/chown02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/chown05.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/clock_gettime01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/fchmod02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fchmod04.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fchown02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/fchown05.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/futex_wake04.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getgid01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/getgroups03.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/getrandom04.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/hugemmap06.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/mkdir05.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/mmap10.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/mmap11.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/nice04.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/open02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/open11.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/open14.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/openat03.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/readlink01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/setgroups01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/setgroups02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/setgroups04.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/setpriority02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/setrlimit03.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/setsockopt03.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/stat01.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/stat01_64.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/futex_wait05.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/poll02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/unlink08.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/bind02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/3/epoll_pwait03.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/3/epoll_wait02.elf.ok
endif

# TODO: Cygwin doesn't properly raise an EACCES errno.
ifneq ($(HOST_OS), Cygwin)
LTP_TESTS += o/$(MODE)/third_party/ltp/bin/2/readlink03.elf.ok
endif

ifneq ($(HOST_OS), Cygwin)
# partial failure only on cygwin
# chmod01.c:50: TFAIL: stat(testfile) mode=0000
# chmod01.c:50: TFAIL: stat(testdir_1) mode=0000
# chmod01.c:50: TFAIL: stat(testdir_1) mode=0777
# chmod01.c:50: TFAIL: stat(testdir_1) mode=4777
LTP_TESTS += o/$(MODE)/third_party/ltp/bin/1/chmod01.elf.ok
endif

################################################################################
# MEDIUM TESTS

LTP_TESTS_MEDIUM =							\
	o/$(MODE)/third_party/ltp/bin/1/clock_nanosleep02.elf.ok	\
	o/$(MODE)/third_party/ltp/bin/1/getcwd04.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/gettimeofday02.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/kill02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/mmap-corruption01.elf.ok	\
	o/$(MODE)/third_party/ltp/bin/1/mmap001.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/mmapstress01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/nanosleep01.elf.ok		\
	o/$(MODE)/third_party/ltp/bin/1/rename14.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/2/mmap2.elf.ok

ifeq ($(shell id -u), 0)
LTP_TESTS_MEDIUM +=							\
	o/$(MODE)/third_party/ltp/bin/2/select02.elf.ok			\
	o/$(MODE)/third_party/ltp/bin/1/pty06.elf.ok
endif

################################################################################

o/$(MODE)/third_party/ltp: $(LTP_TESTS)
	@mkdir -p $(@D)
	@touch $@

o/$(MODE)/third_party/ltp/medium: $(LTP_TESTS_MEDIUM)
	@mkdir -p $(@D)
	@touch $@
