/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "blink/dll.h"

struct Dll *dll_remove(struct Dll *list, struct Dll *elem) {
  if (list == elem) {
    if (list->prev == list) {
      list = 0;
    } else {
      list = list->prev;
    }
  }
  elem->next->prev = elem->prev;
  elem->prev->next = elem->next;
  elem->next = elem;
  elem->prev = elem;
  return list;
}

void dll_splice_after(struct Dll *elem, struct Dll *succ) {
  struct Dll *tmp1, *tmp2;
  tmp1 = elem->next;
  tmp2 = succ->prev;
  elem->next = succ;
  succ->prev = elem;
  tmp2->next = tmp1;
  tmp1->prev = tmp2;
}

struct Dll *dll_make_first(struct Dll *list, struct Dll *elem) {
  if (elem) {
    if (!list) {
      list = elem->prev;
    } else {
      dll_splice_after(list, elem);
    }
  }
  return list;
}

struct Dll *dll_make_last(struct Dll *list, struct Dll *elem) {
  if (elem) {
    dll_make_first(list, elem->next);
    list = elem;
  }
  return list;
}
