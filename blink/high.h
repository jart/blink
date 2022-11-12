#ifndef BLINK_HIGH_H_
#define BLINK_HIGH_H_
#include <stdbool.h>
#include "blink/types.h"

struct High {
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
