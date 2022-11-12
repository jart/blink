#ifndef BLINK_DIS_H_
#define BLINK_DIS_H_
#include <stdbool.h>
#include <stddef.h>
#include "blink/types.h"

#include "blink/loader.h"
#include "blink/x86.h"

#define DIS_MAX_SYMBOL_LENGTH 32

struct DisOp {
  i64 addr;
  u8 size;
  bool active;
  char *s;
};

struct DisOps {
  int i, n;
  struct DisOp *p;
};

struct DisLoad {
  i64 addr;
  int size;
  bool istext;
};

struct DisLoads {
  int i, n;
  struct DisLoad *p;
};

struct DisSym {
  i64 addr;
  int unique;
  int size;
  int name;
  char rank;
  bool iscode;
  bool isabs;
};

struct DisSyms {
  int i, n;
  struct DisSym *p;
  const char *stab;
};

struct DisEdge {
  i64 src;
  i64 dst;
};

struct DisEdges {
  int i, n;
  struct DisEdge *p;
};

struct Dis {
  struct DisOps ops;
  struct DisLoads loads;
  struct DisSyms syms;
  struct DisEdges edges;
  struct XedDecodedInst xedd[1];
  struct Machine *m; /* for the segment registers */
  u64 addr;     /* current effective address */
  char buf[512];
};

extern bool g_disisprog_disable;

long Dis(struct Dis *, struct Machine *, i64, i64, int);
long DisFind(struct Dis *, i64);
void DisFree(struct Dis *);
void DisFreeOp(struct DisOp *);
void DisFreeOps(struct DisOps *);
void DisLoadElf(struct Dis *, struct Elf *);
long DisFindSym(struct Dis *, i64);
long DisFindSymByName(struct Dis *, const char *);
bool DisIsText(struct Dis *, i64);
bool DisIsProg(struct Dis *, i64);
char *DisInst(struct Dis *, char *, const char *);
char *DisArg(struct Dis *, char *, const char *);
const char *DisSpec(struct XedDecodedInst *, char *);
const char *DisGetLine(struct Dis *, struct Machine *, int);

#endif /* BLINK_DIS_H_ */
