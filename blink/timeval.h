#ifndef BLINK_TIMEVAL_H_
#define BLINK_TIMEVAL_H_
#include <sys/time.h>

long timeval_tomicros(struct timeval);
struct timeval timeval_sub(struct timeval, struct timeval);

#endif /* BLINK_TIMEVAL_H_ */
