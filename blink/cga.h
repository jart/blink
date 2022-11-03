#ifndef BLINK_CGA_H_
#define BLINK_CGA_H_
#include <stdint.h>

#include "blink/panel.h"

void DrawCga(struct Panel *, uint8_t[25][80][2]);
size_t FormatCga(uint8_t, char[static 11]);

#endif /* BLINK_CGA_H_ */
