// checks for vasprintf() function
// available in gnu/bsd/mac/cygwin
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *xasprintf(const char *fmt, ...) {
  int rc;
  char *s;
  va_list va;
  va_start(va, fmt);
  rc = vasprintf(&s, fmt, va);
  va_end(va);
  if (rc == -1) exit(1);         // vasprintf failed
  if (rc != strlen(s)) exit(2);  // vasprintf broken
  return s;
}

int main(int argc, char *argv[]) {
  char *s;
  s = xasprintf("hello %d", 123);
  if (strcmp(s, "hello 123")) exit(3);
  free(s);
  return 0;
}
