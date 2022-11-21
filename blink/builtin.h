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

#ifdef __cplusplus
#define forceinline inline
#else
#if !defined(__STRICT_ANSI__) && \
    (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 302
#if (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 403 || \
    !defined(__cplusplus) ||                              \
    (defined(__clang__) &&                                \
     (defined(__GNUC_STDC_INLINE__) || defined(__GNUC_GNU_INLINE__)))
#if defined(__GNUC_STDC_INLINE__) || defined(__cplusplus)
#define forceinline                                                 \
  static __inline __attribute__((__always_inline__, __gnu_inline__, \
                                 __no_instrument_function__, __unused__))
#else
#define forceinline              \
  static __inline __attribute__( \
      (__always_inline__, __no_instrument_function__, __unused__))
#endif /* __GNUC_STDC_INLINE__ */
#endif /* GCC >= 4.3 */
#elif defined(_MSC_VER)
#define forceinline __forceinline
#else
#define forceinline static inline
#endif /* !ANSI && GCC >= 3.2 */
#endif /* __cplusplus */

#endif /* BLINK_BUILTIN_H_ */
