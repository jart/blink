#ifndef BLINK_BUFFER_H_
#define BLINK_BUFFER_H_
#include <sys/types.h>
#include <wchar.h>

#include "blink/builtin.h"

struct Buffer {
  int i, n;
  char *p;
};

int AppendChar(struct Buffer *, char);
void AppendData(struct Buffer *, const char *, int);
int AppendStr(struct Buffer *, const char *);
int AppendWide(struct Buffer *, wint_t);
int AppendFmt(struct Buffer *, const char *, ...) printf_attr(2);

#endif /* BLINK_BUFFER_H_ */
