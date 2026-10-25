#ifndef BLINK_ROOTLESS_H_
#define BLINK_ROOTLESS_H_
#include <sys/stat.h>
#include <sys/types.h>
#include "blink/atomic.h"

/* Opt-in build metadata, not a security boundary or permission emulator. */
enum RootlessId { ROOTLESS_REAL, ROOTLESS_EFFECTIVE, ROOTLESS_SAVED, ROOTLESS_FS };
unsigned RootlessGetId(int, enum RootlessId);
void RootlessGetIds(int, unsigned[4]);
int RootlessSetId(int, unsigned);
int RootlessSetReId(int, unsigned, unsigned);
int RootlessSetResId(int, unsigned, unsigned, unsigned);
unsigned RootlessSetFsId(int, unsigned);
int RootlessEnabled(void);
int RootlessStat(int, const char *, struct stat *, int);
int RootlessChown(int, const char *, unsigned, unsigned, int);
int RootlessChmod(int, const char *, unsigned, int);
struct RootlessOwner { unsigned uid, gid; int setgid; };
int RootlessPrepare(int, const char *, struct RootlessOwner *);
int RootlessCreate(int, const char *, int, const struct RootlessOwner *, unsigned);
struct RootlessAnon;
struct RootlessAnon *RootlessAnonNew(unsigned);
struct RootlessAnon *RootlessAnonAcquire(struct RootlessAnon *);
void RootlessAnonFree(struct RootlessAnon *);
void RootlessAnonStat(struct RootlessAnon *, struct stat *);
int RootlessAnonChown(struct RootlessAnon *, unsigned, unsigned);
int RootlessAnonChmod(struct RootlessAnon *, unsigned);
int RootlessGetgroups(int, unsigned *);
int RootlessSetgroups(int, const unsigned *);
#endif
