#ifndef BLINK_DLL_H_
#define BLINK_DLL_H_
#include <stddef.h>

#define DLL_CONTAINER(t, f, p) ((t *)(((char *)(p)) - offsetof(t, f)))

struct Dll {
  struct Dll *next;
  struct Dll *prev;
};

static inline void dll_init(struct Dll *e) {
  e->next = e;
  e->prev = e;
}

static inline int dll_is_empty(struct Dll *list) {
  return !list;
}

static inline struct Dll *dll_last(struct Dll *list) {
  return list;
}

static inline struct Dll *dll_first(struct Dll *list) {
  struct Dll *first = 0;
  if (list) first = list->next;
  return first;
}

static inline struct Dll *dll_next(struct Dll *list, struct Dll *e) {
  struct Dll *next = 0;
  if (e != list) next = e->next;
  return next;
}

static inline struct Dll *dll_prev(struct Dll *list, struct Dll *e) {
  struct Dll *prev = 0;
  if (e != list->next) prev = e->prev;
  return prev;
}

struct Dll *dll_make_first(struct Dll *, struct Dll *);
struct Dll *dll_make_last(struct Dll *, struct Dll *);
struct Dll *dll_remove(struct Dll *, struct Dll *);
void dll_splice_after(struct Dll *, struct Dll *);

#endif /* BLINK_DLL_H_ */
