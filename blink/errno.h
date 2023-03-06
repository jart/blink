#ifndef BLINK_ERRNO_H_
#define BLINK_ERRNO_H_

long eagain(void);
long ebadf(void);
long efault(void);
void *efault0(void);
long eintr(void);
long einval(void);
long enfile(void);
long enoent(void);
long enomem(void);
long enosys(void);
long emfile(void);
long enotdir(void);
long enotsup(void);
long eoverflow(void);
long eopnotsupp(void);
long erange(void);
long eperm(void);
long esrch(void);

#endif /* BLINK_ERRNO_H_ */
