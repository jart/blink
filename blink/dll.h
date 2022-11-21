#ifndef BLINK_DLL_H_
#define BLINK_DLL_H_
#include <stddef.h>

#define DLL_CONTAINER(t, f, p) ((t *)(((char *)(p)) - offsetof(t, f)))

typedef struct dll_element_s {
  struct dll_element_s *next;
  struct dll_element_s *prev;
} dll_element;

typedef dll_element *dll_list;

static inline void dll_init(dll_element *e) {
  e->next = e;
  e->prev = e;
}

static inline int dll_is_empty(dll_list list) {
  return !list;
}

static inline dll_element *dll_last(dll_list list) {
  return list;
}

static inline dll_element *dll_first(dll_list list) {
  dll_element *first = 0;
  if (list) first = list->next;
  return first;
}

static inline dll_element *dll_next(dll_list list, dll_element *e) {
  dll_element *next = 0;
  if (e != list) next = e->next;
  return next;
}

static inline dll_element *dll_prev(dll_list list, dll_element *e) {
  dll_element *prev = 0;
  if (e != list->next) prev = e->prev;
  return prev;
}

dll_list dll_remove(dll_list, dll_element *);
void dll_splice_after(dll_element *, dll_element *);
dll_list dll_make_first(dll_list, dll_element *);
dll_list dll_make_last(dll_list, dll_element *);

#endif /* BLINK_DLL_H_ */
