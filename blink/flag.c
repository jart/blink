/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2023 Justine Alexandra Roberts Tunney                              │
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
#include "blink/builtin.h"
#include "blink/flag.h"

bool FLAG_zero;
bool FLAG_wantjit;
bool FLAG_nolinear;
bool FLAG_noconnect;
bool FLAG_nologstderr;
bool FLAG_alsologtostderr;

int FLAG_strace;
int FLAG_vabits;

u64 FLAG_skew;
u64 FLAG_vaspace;
u64 FLAG_aslrmask;
u64 FLAG_stacktop;
u64 FLAG_imagestart;
u64 FLAG_automapend;
u64 FLAG_automapstart;
u64 FLAG_dyninterpaddr;

const char *FLAG_logpath;

#ifndef DISABLE_OVERLAYS
const char *FLAG_overlays;
#endif
