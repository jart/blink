#ifndef BLINK_OVERLAYS_H_
#define BLINK_OVERLAYS_H_
#include <sys/stat.h>

void SetOverlays(const char *);
int OverlaysOpen(int, const char *, int, int);
int OverlaysStat(int, const char *, struct stat *, int);

#endif /* BLINK_OVERLAYS_H_ */
