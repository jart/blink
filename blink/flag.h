#ifndef BLINK_FLAG_H_
#define BLINK_FLAG_H_
#include <stdbool.h>

#include "blink/types.h"

extern bool FLAG_zero;
extern bool FLAG_wantjit;
extern bool FLAG_nolinear;
extern bool FLAG_noconnect;
extern bool FLAG_nologstderr;
extern bool FLAG_alsologtostderr;

extern int FLAG_strace;
extern int FLAG_vabits;

extern long FLAG_pagesize;

extern u64 FLAG_skew;
extern u64 FLAG_vaspace;
extern u64 FLAG_stacktop;
extern u64 FLAG_aslrmask;
extern u64 FLAG_imagestart;
extern u64 FLAG_automapend;
extern u64 FLAG_automapstart;
extern u64 FLAG_dyninterpaddr;

extern const char *FLAG_logpath;
extern const char *FLAG_overlays;

#endif /* BLINK_FLAG_H_ */
