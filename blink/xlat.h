#ifndef BLINK_XLAT_H_
#define BLINK_XLAT_H_
#include <netinet/in.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "blink/linux.h"
#include "blink/termios.h"

int UnXlatOpenFlags(int);
int UnXlatAccMode(int);
int UnXlatSignal(int);
int XlatAccess(int);
int XlatAtf(int);
int XlatClock(int);
int XlatErrno(int);
int XlatLock(int);
int XlatMapFlags(int);
int XlatOpenFlags(int);
int XlatAccMode(int);
int XlatResource(int);
int XlatRusage(int);
int XlatShutdown(int);
int XlatSig(int);
int XlatSignal(int);
int XlatSocketFamily(int);
int XlatSocketLevel(int);
int XlatSocketOptname(int, int);
int XlatSocketProtocol(int);
int XlatSocketType(int);
int XlatWait(int);
int XlatWhence(int);

int XlatSockaddrToHost(struct sockaddr_in *, const struct sockaddr_in_linux *);
void XlatSockaddrToLinux(struct sockaddr_in_linux *,
                         const struct sockaddr_in *);
void XlatStatToLinux(struct stat_linux *, const struct stat *);
void XlatRusageToLinux(struct rusage_linux *, const struct rusage *);
void XlatItimervalToLinux(struct itimerval_linux *, const struct itimerval *);
void XlatLinuxToItimerval(struct itimerval *, const struct itimerval_linux *);
void XlatLinuxToTermios(struct termios *, const struct termios_linux *);
void XlatTermiosToLinux(struct termios_linux *, const struct termios *);
void XlatWinsizeToLinux(struct winsize_linux *, const struct winsize *);
void XlatSigsetToLinux(u8[8], const sigset_t *);
void XlatLinuxToSigset(sigset_t *, const u8[8]);
void XlatRlimitToLinux(struct rlimit_linux *, const struct rlimit *);
void XlatLinuxToRlimit(struct rlimit *, const struct rlimit_linux *);

#endif /* BLINK_XLAT_H_ */
