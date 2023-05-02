#ifndef BLINK_BLINKENLIGHTS_H_
#define BLINK_BLINKENLIGHTS_H_
/* Shared variables and functions with bios.c */

#include <stdbool.h>
#include <stdlib.h>
#include <sys/types.h>

#define kModePty 255
#define EXIT     0x400

/* blinkenlights.c */
extern int ttyin;
extern int vidya;
extern int action;
extern int exitcode;
extern bool tuimode;
extern struct Pty *pty;
extern struct Machine *m;
extern bool ptyisenabled;

void SetCarry(bool cf);
void ReactiveDraw(void);
void Redraw(bool force);
void DrawDisplayOnly(void);
bool HasPendingKeyboard(void);
void HandleAppReadInterrupt(void);
ssize_t ReadAnsi(int fd, char *p, size_t n);

/* bios.c */
void VidyaServiceSetMode(int);
bool OnCallBios(int interrupt);

#endif /* BLINK_BLINKENLIGHTS_H_ */
