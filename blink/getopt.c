/*	$NetBSD: getopt.c,v 1.26 2003/08/07 16:43:40 agc Exp $	*/
/*
 * Copyright (c) 1987, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * @(#)getopt.c	8.3 (Berkeley) 4/27/95
 * $FreeBSD: src/lib/libc/stdlib/getopt.c,v 1.8 2007/01/09 00:28:10 imp Exp $
 * $DragonFly: src/lib/libc/stdlib/getopt.c,v 1.7 2005/11/20 12:37:48 swildner
 */
#include <string.h>
#include <unistd.h>

/**
 * @fileoverview Command Line Argument Parser
 *
 * We vendor this ancient NetBSD getopt() implementation because:
 *
 * 1. We want the non-GNU behavior where flags must come before the
 *    first argument. GNU systems permit flags to come after arguments,
 *    which isn't a great fit for commands like `blink` whose arguments
 *    are mostly just passed along to a sub-command.
 *
 * 2. We've modified it by hand to have a tiny transitive footprint. The
 *    `blink` command can't depend on beefy libc functions like printf()
 *    or even snprintf().
 */

#define BADCH  '?'
#define BADARG ':'

int optind_;
char *optarg_;

static char *getopt_place;
static char getopt_emsg[1];

static void getopt_print_badch(int argc, char *const argv[], int optopt,
                               const char *s) {
  long i = 0;
  char b[256], *t;
  if (argc > 0 && (t = argv[0])) {
    while (*t && i < sizeof(b) - 64) b[i++] = *t++;
    b[i++] = ':';
    b[i++] = ' ';
  }
  while (*s) b[i++] = *s++;
  b[i + 0] = ' ';
  b[i + 1] = '-';
  b[i + 2] = '-';
  b[i + 3] = ' ';
  b[i + 4] = optopt;
  b[i + 5] = '\n';
  write(2, b, i + 6);
}

/**
 * Parses argc/argv argument vector, e.g.
 *
 *     while ((opt = getopt(argc, argv, "hvx:")) != -1) {
 *       switch (opt) {
 *         case 'x':
 *           x = atoi(optarg_);
 *           break;
 *         case 'v':
 *           ++verbose;
 *           break;
 *         case 'h':
 *           PrintUsage(EXIT_SUCCESS, stdout);
 *         default:
 *           PrintUsage(EX_USAGE, stderr);
 *       }
 *     }
 *
 * @see optind_
 * @see optarg_
 */
int getopt_(int nargc, char *const nargv[], const char *ostr) {
  int optopt;
  const char *oli; /* option letter list index */
  if (!optind_) optind_ = 1;
  if (!getopt_place) getopt_place = getopt_emsg;
  if (!*getopt_place) { /* update scanning pointer */
    getopt_place = nargv[optind_];
    if (optind_ >= nargc || *getopt_place++ != '-') {
      /* Argument is absent or is not an option */
      getopt_place = getopt_emsg;
      return -1;
    }
    optopt = *getopt_place++;
    if (optopt == '-' && *getopt_place == 0) {
      /* "--" => end of options */
      ++optind_;
      getopt_place = getopt_emsg;
      return -1;
    }
    if (optopt == 0) {
      /* Solitary '-', treat as a '-' option
         if the program (eg su) is looking for it. */
      getopt_place = getopt_emsg;
      if (!strchr(ostr, '-')) return -1;
      optopt = '-';
    }
  } else {
    optopt = *getopt_place++;
  }
  /* See if option letter is one the caller wanted... */
  if (optopt == ':' || !(oli = strchr(ostr, optopt))) {
    if (*getopt_place == 0) ++optind_;
    if (*ostr != ':')
      getopt_print_badch(nargc, nargv, optopt, "illegal option");
    return BADCH;
  }
  /* Does this option need an argument? */
  if (oli[1] != ':') {
    /* don't need argument */
    optarg_ = 0;
    if (*getopt_place == 0) ++optind_;
  } else {
    /* Option-argument is either the rest of this argument or the
       entire next argument. */
    if (*getopt_place) {
      optarg_ = getopt_place;
    } else if (nargc > ++optind_) {
      optarg_ = nargv[optind_ & 0xffffffff];
    } else {
      /* option-argument absent */
      getopt_place = getopt_emsg;
      if (*ostr == ':') return BADARG;
      getopt_print_badch(nargc, nargv, optopt, "option requires an argument");
      return BADCH;
    }
    getopt_place = getopt_emsg;
    ++optind_;
  }
  return optopt; /* return option letter */
}
