#ifndef BLINK_BLINKENLIGHTS_H_
#define BLINK_BLINKENLIGHTS_H_
/* Shared variables and functions with bios.c */

#include <stdbool.h>
#include <stdlib.h>
#include <wchar.h>
#include <sys/types.h>

#include "blink/likely.h"
#include "blink/util.h"

#define kModePty 255

/* blinkenlights.c */
extern int ttyin;
extern int vidya;
extern bool tuimode;
extern struct Pty *pty;
extern struct Machine *m;
extern bool ptyisenabled;

void SetCarry(bool cf);
void ReactiveDraw(void);
void Redraw(bool force);
void DrawDisplayOnly(void);
bool HasPendingKeyboard(void);
void HandleAppReadInterrupt(bool errflag);
ssize_t ReadAnsi(int fd, char *p, size_t n);

/* bios.c */
void VidyaServiceSetMode(int);
bool OnCallBios(int interrupt);

static inline wint_t GetVidyaByte(unsigned char b) {
  if (LIKELY(0x20 <= b && b <= 0x7E)) return b;
  /*
   * In the emulated screen, show 0xff as a "shouldered open box"
   * instead of lambda.  The Unicode 4.0 charts tell us that this
   * glyph is an ISO 9995-7 "keyboard symbol for No Break Space".
   */
  if (UNLIKELY(b == 0xFF)) return 0x237D;
  return kCp437[b];
}

#endif /* BLINK_BLINKENLIGHTS_H_ */
