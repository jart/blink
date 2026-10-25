#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/vfs.h>
#include <unistd.h>

/* Run inside a VFS root to catch host-path and host-fd bypasses. */
int main(void) {
  char dir[] = "/tmp/blink-statfs-XXXXXX";
  char link[128], missing[128];
  struct statfs bypath, byfd, bylink;
  int fd = -1, rc = 0;
  if (!mkdtemp(dir)) return 1;
  snprintf(link, sizeof(link), "%s/link", dir);
  snprintf(missing, sizeof(missing), "%s/missing", dir);
  if (statfs(dir, &bypath)) { rc = 2; goto done; }
  if ((fd = open(dir, O_RDONLY | O_DIRECTORY)) == -1) { rc = 3; goto done; }
  if (fstatfs(fd, &byfd)) { rc = 4; goto done; }
  if (!bypath.f_bsize || bypath.f_bsize != byfd.f_bsize ||
      bypath.f_type != byfd.f_type || bypath.f_blocks != byfd.f_blocks) {
    rc = 5;
    goto done;
  }
  if (symlink(".", link) || statfs(link, &bylink)) { rc = 6; goto done; }
  if (bylink.f_type != bypath.f_type || bylink.f_blocks != bypath.f_blocks) {
    rc = 7;
    goto done;
  }
  errno = 0;
  if (statfs(missing, &bylink) != -1 || errno != ENOENT) rc = 8;
  errno = 0;
  if (fstatfs(-1, &bylink) != -1 || errno != EBADF) rc = 9;
done:
  if (fd != -1) close(fd);
  unlink(link);
  rmdir(dir);
  return rc;
}
