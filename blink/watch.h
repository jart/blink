#ifndef BLINK_WATCH_H_
#define BLINK_WATCH_H_
#include <sys/types.h>

#include "blink/machine.h"
#include "blink/types.h"

struct Watchpoint {
  i64 addr;
  const char *symbol;
  u64 oldvalue;
  bool initialized;
  bool disable;
};

struct Watchpoints {
  int i, n;
  struct Watchpoint *p;
};

ssize_t IsAtWatchpoint(struct Watchpoints *, struct Machine *);
ssize_t PushWatchpoint(struct Watchpoints *, struct Watchpoint *);
void PopWatchpoint(struct Watchpoints *);

#endif /* BLINK_WATCH_H_ */
