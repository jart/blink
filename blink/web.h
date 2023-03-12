#ifndef BLINK_WEB_H_
#define BLINK_WEB_H_
#include "blink/builtin.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>

static void SetupWeb(void) {
  // EM_ASM({FS.mkdir("/cwd"); FS.mount(NODEFS, {root : "."}, "/cwd");});
}

#else
static void SetupWeb(void) {
}
#endif /* __EMSCRIPTEN__ */
#endif /* BLINK_WEB_H_ */
