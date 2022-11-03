#ifndef BLINK_BREAKPOINT_H_
#define BLINK_BREAKPOINT_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

struct Breakpoints {
  size_t i, n;
  struct Breakpoint {
    int64_t addr;
    const char *symbol;
    bool disable;
    bool oneshot;
  } * p;
};

ssize_t IsAtBreakpoint(struct Breakpoints *, int64_t);
ssize_t PushBreakpoint(struct Breakpoints *, struct Breakpoint *);
void PopBreakpoint(struct Breakpoints *);

#endif /* BLINK_BREAKPOINT_H_ */
