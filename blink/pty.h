#ifndef BLINK_PTY_H_
#define BLINK_PTY_H_
#include "blink/buffer.h"
#include "blink/types.h"

#define kPtyFg      0x0001
#define kPtyBg      0x0002
#define kPtyBold    0x0004
#define kPtyFlip    0x0008
#define kPtyFaint   0x0010
#define kPtyUnder   0x0020
#define kPtyDunder  0x0040
#define kPtyTrue    0x0080
#define kPtyBlink   0x0100
#define kPtyItalic  0x0200
#define kPtyFraktur 0x0400
#define kPtyStrike  0x0800
#define kPtyConceal 0x1000

#define kPtyBell        0x001
#define kPtyRedzone     0x002
#define kPtyNocursor    0x004
#define kPtyBlinkcursor 0x008
#define kPtyNocanon     0x010
#define kPtyNoecho      0x020
#define kPtyNoopost     0x040
#define kPtyLed1        0x080
#define kPtyLed2        0x100
#define kPtyLed3        0x200
#define kPtyLed4        0x400

enum PtyState {
  kPtyAscii,
  kPtyUtf8,
  kPtyEsc,
  kPtyCsi,
};

struct PtyEsc {
  unsigned i;
  char s[64];
};

struct Pty {
  int y;
  int x;
  int yn;
  int xn;
  u32 u8;
  u32 n8;
  u32 pr;
  u32 fg;
  u32 bg;
  u32 conf;
  u32 save;
  u32 *wcs;
  u32 *prs;
  u32 *fgs;
  u32 *bgs;
  wchar_t *xlat;
  enum PtyState state;
  struct PtyEsc esc;
  struct Buffer input;
};

void FreePty(struct Pty *);
struct Pty *NewPty(void);
void PtyResize(struct Pty *, int, int);
ssize_t PtyRead(struct Pty *, void *, size_t);
ssize_t PtyWrite(struct Pty *, const void *, size_t);
ssize_t PtyWriteInput(struct Pty *, const void *, size_t);
int PtyAppendLine(struct Pty *, struct Buffer *, unsigned);
void PtyFullReset(struct Pty *);
void PtyMemmove(struct Pty *, long, long, long);
void PtyErase(struct Pty *, long, long);
void PtySetY(struct Pty *, int);
void PtySetX(struct Pty *, int);

#endif /* BLINK_PTY_H_ */
