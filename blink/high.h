#ifndef BLINK_HIGH_H_
#define BLINK_HIGH_H_
#include <stdbool.h>

#include "blink/types.h"

#define DISABLE_HIGHLIGHT_BEGIN \
  {                             \
    bool high_;                 \
    high_ = g_high.enabled;     \
    g_high.enabled = false
#define DISABLE_HIGHLIGHT_END \
  g_high.enabled = high_;     \
  }

struct High {
  bool enabled;
  bool active;
  u8 keyword;
  u8 reg;
  u8 literal;
  u8 label;
  u8 comment;
  u8 quote;
};

extern struct High g_high;

char *HighStart(char *, int);
char *HighEnd(char *);

#endif /* BLINK_HIGH_H_ */
