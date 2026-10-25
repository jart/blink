#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void) {
#ifdef __x86_64__
  char tmp[] = "/tmp/blink-fchmodat2-XXXXXX";
  struct stat st;
  int dir, file;
  if (!mkdtemp(tmp) || (dir = open(tmp, O_RDONLY | O_DIRECTORY)) < 0) return 1;
  if (mkdirat(dir, "directory", 0700)) return 2;
  if (syscall(452, dir, "directory", 0751, AT_SYMLINK_NOFOLLOW)) return 3;
  if (fstatat(dir, "directory", &st, 0) || (st.st_mode & 0777) != 0751) return 4;
  if ((file = openat(dir, "file", O_CREAT | O_RDWR, 0600)) < 0) return 5;
  if (syscall(452, file, "", 0640, 0x1000) || fstat(file, &st) ||
      (st.st_mode & 0777) != 0640) return 6;
  if (symlinkat("file", dir, "link")) return 7;
  errno = 0;
  if (syscall(452, dir, "link", 0777, AT_SYMLINK_NOFOLLOW) != -1 ||
      errno != EOPNOTSUPP || fstat(file, &st) || (st.st_mode & 0777) != 0640) return 8;
  if (syscall(452, dir, "link", 0604, 0) || fstat(file, &st) ||
      (st.st_mode & 0777) != 0604) return 9;
  errno = 0;
  if (syscall(452, dir, "directory", 0700, 0x40000000) != -1 || errno != EINVAL) return 10;
  errno = 0;
  if (syscall(452, dir, "", 0700, 0) != -1 || errno != ENOENT) return 11;
  errno = 0;
  if (syscall(452, -1, "", 0700, 0x1000) != -1 || errno != EBADF) return 12;
  if (fchdir(dir) || syscall(452, AT_FDCWD, "", 0701, 0x1000) ||
      fstat(dir, &st) || (st.st_mode & 0777) != 0701) return 13;
  close(file);
  unlinkat(dir, "file", 0); unlinkat(dir, "link", 0);
  unlinkat(dir, "directory", AT_REMOVEDIR);
  chdir("/"); close(dir); rmdir(tmp);
#endif
  return 0;
}
