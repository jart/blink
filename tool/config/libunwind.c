// checks for libunwind backtrace support
#define UNW_LOCAL_ONLY
#ifdef __x86_64__
#include <libunwind-x86_64.h>
#else
#include <libunwind.h>
#endif
#include <stdio.h>
#include <string.h>

#define APPEND(...) o += snprintf(b + o, n - o, __VA_ARGS__)

const char *GetBacktrace(void) {
  static char b[2048];
  int o = 0;
  char sym[256];
  int n = sizeof(b);
  unw_cursor_t cursor;
  unw_context_t context;
  unw_word_t offset, pc;
  unw_getcontext(&context);
  unw_init_local(&cursor, &context);
  APPEND("backtrace");
  while (unw_step(&cursor) > 0) {
    unw_get_reg(&cursor, UNW_REG_IP, &pc);
    if (!pc) break;
    APPEND("\n\t%lx ", pc);
    if (unw_get_proc_name(&cursor, sym, sizeof(sym), &offset) == 0) {
      APPEND("%s+%ld", sym, offset);
    } else {
      APPEND("<unknown>");
    }
  }
  return b;
}

const char *MyApp(void) {
  return GetBacktrace();
}

const char *(*MyAppPtr)(void) = MyApp;

int main(int argc, char *argv[]) {
  const char *bt;
  bt = MyAppPtr();
  if (!strstr(bt, "MyApp")) return 1;
  if (!strstr(bt, "main")) return 2;
  return 0;
}
