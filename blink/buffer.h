#ifndef BLINK_BUFFER_H_
#define BLINK_BUFFER_H_
#include <sys/types.h>
#include <wchar.h>

struct Buffer {
  int i, n;
  char *p;
};

void AppendChar(struct Buffer *, char);
void AppendData(struct Buffer *, const char *, int);
int AppendStr(struct Buffer *, const char *);
void AppendWide(struct Buffer *, wint_t);
int AppendFmt(struct Buffer *, const char *, ...);

#endif /* BLINK_BUFFER_H_ */
