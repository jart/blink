#ifndef BLINK_TIMESPEC_H_
#define BLINK_TIMESPEC_H_
#include <time.h>

int timespec_cmp(struct timespec, struct timespec);
struct timespec timespec_add(struct timespec, struct timespec);
struct timespec timespec_sub(struct timespec, struct timespec);

#endif /* BLINK_TIMESPEC_H_ */
