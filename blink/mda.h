#ifndef BLINK_MDA_H_
#define BLINK_MDA_H_
#include <stdint.h>

#include "blink/panel.h"

void DrawMda(struct Panel *, uint8_t[25][80][2]);

#endif /* BLINK_MDA_H_ */
