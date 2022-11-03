#ifndef BLINK_HIGH_H_
#define BLINK_HIGH_H_
#include <stdbool.h>
#include <stdint.h>

struct High {
  bool active;
  uint8_t keyword;
  uint8_t reg;
  uint8_t literal;
  uint8_t label;
  uint8_t comment;
  uint8_t quote;
};

extern struct High g_high;

char *HighStart(char *, int);
char *HighEnd(char *);

#endif /* BLINK_HIGH_H_ */
