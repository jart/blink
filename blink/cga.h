#ifndef BLINK_CGA_H_
#define BLINK_CGA_H_
#include "blink/panel.h"
#include "blink/types.h"

void DrawCga(struct Panel *, u8[25][80][2], int, int);
size_t FormatCga(u8, char[11]);

#endif /* BLINK_CGA_H_ */
