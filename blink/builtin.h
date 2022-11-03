#ifndef BLINK_BUILTIN_H_
#define BLINK_BUILTIN_H_

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#ifdef _MSC_VER
#define __builtin_unreachable() __assume(0)
#elif !((__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 405 || \
        defined(__clang__) || defined(__INTEL_COMPILER))
#define __builtin_unreachable() \
  for (;;) {                    \
  }
#endif

#if !(defined(__GNUC__) && (__GNUC__ > 2) && defined(__OPTIMIZE__))
#define __builtin_expect(x, y) (x)
#endif

#ifdef _MSC_VER
#define dontinline __declspec(noinline)
#elif __has_attribute(__noinline__) || \
    (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 301
#define dontinline __attribute__((__noinline__))
#else
#define dontinline
#endif

#endif /* BLINK_BUILTIN_H_ */
