#ifndef BLINK_FLAG_H_
#define BLINK_FLAG_H_
#include <stdbool.h>

extern int FLAG_strace;
extern bool FLAG_nolinear;
extern bool FLAG_noconnect;
extern bool FLAG_nologstderr;
extern bool FLAG_alsologtostderr;
extern const char *FLAG_logpath;
extern const char *FLAG_overlays;

#endif /* BLINK_FLAG_H_ */
