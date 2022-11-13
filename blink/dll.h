#ifndef BLINK_DLL_H_
#define BLINK_DLL_H_
#include <stddef.h>

#define DLL_CONTAINER(t, f, p) ((t *)(((char *)(p)) - offsetof(t, f)))

typedef struct dll_element_s {
  struct dll_element_s *next;
  struct dll_element_s *prev;
} dll_element;

typedef dll_element *dll_list;

void dll_init(dll_element *);
int dll_is_empty(dll_list);
dll_list dll_remove(dll_list, dll_element *);
void dll_splice_after(dll_element *, dll_element *);
dll_list dll_make_first(dll_list, dll_element *);
dll_list dll_make_last(dll_list, dll_element *);
dll_element *dll_first(dll_list);
dll_element *dll_last(dll_list);
dll_element *dll_next(dll_list, dll_element *);
dll_element *dll_prev(dll_list, dll_element *);

#endif /* BLINK_DLL_H_ */
