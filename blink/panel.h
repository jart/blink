#ifndef BLINK_PANEL_H_
#define BLINK_PANEL_H_
#include <stddef.h>
#include <sys/types.h>

#include "blink/buffer.h"

struct Panel {
  int top;
  int bottom;
  int left;
  int right;
  struct Buffer *lines;
  int n;
};

ssize_t PrintPanels(int, long, struct Panel *, long, long);
void PrintMessageBox(int, const char *, long, long);

#endif /* BLINK_PANEL_H_ */
