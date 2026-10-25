#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
  const char *headers[] = {"#!/bin/sh -e\n", "#! /bin/sh -e\n",
                           "#!\t /bin/sh\t-e \t\n"};
  char dir[] = "/tmp/blink-shebang-XXXXXX", script[128];
  int i, pid, status, rc = 0;
  FILE *f;
  if (!mkdtemp(dir)) return 1;
  snprintf(script, sizeof(script), "%s/script", dir);
  for (i = 0; i < 3; ++i) {
    if (!(f = fopen(script, "w"))) { rc = 2; break; }
    fputs(headers[i], f);
    fputs("test \"$(uname -s)\" = Linux\n"
          "test \"$1\" = passed\n"
          "case \"$-\" in *e*) exit 0;; *) exit 42;; esac\n", f);
    if (fclose(f) || chmod(script, 0700)) { rc = 3; break; }
    if ((pid = fork()) == -1) { rc = 4; break; }
    if (!pid) {
      execl(script, script, "passed", (char *)0);
      _exit(127);
    }
    if (waitpid(pid, &status, 0) != pid || !WIFEXITED(status) || WEXITSTATUS(status)) {
      rc = 10 + i;
      break;
    }
  }
  unlink(script);
  rmdir(dir);
  return rc;
}
