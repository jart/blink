#ifndef BLINK_OVERLAYS_H_
#define BLINK_OVERLAYS_H_
#include <sys/stat.h>
#include <unistd.h>

void SetOverlays(const char *);
int OverlaysChdir(const char *);
char *OverlaysGetcwd(char *, size_t);
int OverlaysOpen(int, const char *, int, int);
int OverlaysStat(int, const char *, struct stat *, int);
int OverlaysAccess(int, const char *, int, int);
int OverlaysUnlink(int, const char *, int);
int OverlaysChmod(int, const char *, int, int);
int OverlaysMkdir(int, const char *, int);
int OverlaysChown(int, const char *, uid_t, gid_t, int);

#endif /* BLINK_OVERLAYS_H_ */
