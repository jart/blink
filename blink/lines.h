#ifndef BLINK_LINES_H_
#define BLINK_LINES_H_
#include <stddef.h>

struct Lines {
  size_t n;
  char **p;
};

struct Lines *NewLines(void);
void FreeLines(struct Lines *);
void AppendLine(struct Lines *, const char *, unsigned);
void AppendLines(struct Lines *, const char *);

#endif /* BLINK_LINES_H_ */
