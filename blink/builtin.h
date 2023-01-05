#ifndef BLINK_BUILTIN_H_
#define BLINK_BUILTIN_H_

#if __GNUC__ + 0 < 2
#undef __GNUC__
#elif defined(__GNUC__) && defined(SWIG) /* lool */
#undef __GNUC__
#elif defined(__GNUC__) && defined(__NVCC__) /* lool */
#undef __GNUC__
#elif !defined(__GNUC__) && defined(__APPLE__) /* modesty */
#define __GNUC__            4
#define __GNUC_MINOR__      2
#define __GNUC_PATCHLEVEL__ 1
#elif !defined(__GNUC__) && defined(__TINYC__)
#define __GNUC__            2
#define __GNUC_MINOR__      0
#define __GNUC_PATCHLEVEL__ 0
#endif

#if __GNUC__ + 0 < 2
#define __attribute__(x)
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0
#endif

#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#ifndef __has_feature
#define __has_feature(x) 0
#endif

#if !defined(__SANITIZE_THREAD__) && __has_feature(thread_sanitizer)
#define __SANITIZE_THREAD__
#endif

#if !defined(__SANITIZE_ADDRESS__) && __has_feature(address_sanitizer)
#define __SANITIZE_ADDRESS__
#endif

#ifndef __BIGGEST_ALIGNMENT__
#define __BIGGEST_ALIGNMENT__ 16
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

#if !__has_builtin(__builtin___clear_cache) && \
    (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) < 403
#define __builtin___clear_cache(x, y) (void)0
#endif

#ifndef pureconst
#ifndef __STRICT_ANSI__
#define pureconst __attribute__((__const__))
#else
#define pureconst
#endif
#endif

#ifndef dontinline
#ifdef _MSC_VER
#define dontinline __declspec(noinline)
#elif __has_attribute(__noinline__) || \
    (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 301
#define dontinline __attribute__((__noinline__))
#else
#define dontinline
#endif
#endif

#ifndef nosideeffect
#if __has_attribute(__pure__) || \
    (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 296
#define nosideeffect __attribute__((__pure__))
#else
#define nosideeffect
#endif
#endif

#ifndef forceinline
#ifdef __cplusplus
#define forceinline inline
#else
#if (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 302
#if (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 403 || \
    !defined(__cplusplus) ||                              \
    (defined(__clang__) &&                                \
     (defined(__GNUC_STDC_INLINE__) || defined(__GNUC_GNU_INLINE__)))
#if defined(__GNUC_STDC_INLINE__) || defined(__cplusplus)
#define forceinline                                                 \
  static __inline __attribute__((__always_inline__, __gnu_inline__, \
                                 __no_instrument_function__, __unused__))
#else
#define forceinline                                 \
  static __inline __attribute__((__always_inline__, \
                                 __no_instrument_function__, __unused__))
#endif /* __GNUC_STDC_INLINE__ */
#endif /* GCC >= 4.3 */
#elif defined(_MSC_VER)
#define forceinline __forceinline
#else
#define forceinline static inline
#endif /* !ANSI && GCC >= 3.2 */
#endif /* __cplusplus */
#endif

#ifndef relegated
#if __has_attribute(__cold__) || \
    (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 403
#define relegated __attribute__((__cold__))
#else
#define relegated
#endif
#endif

#ifndef returnsaligned
#if __has_attribute(__assume_aligned__) || \
    (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 409
#define returnsaligned(x) __attribute__((__assume_aligned__ x))
#else
#define returnsaligned(x)
#endif
#endif

#ifndef optimizesize
#if (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 407 || \
    __has_attribute(__optimize__)
#define optimizesize __attribute__((__optimize__("s")))
#else
#define optimizesize
#endif
#endif

#ifndef dontclone
#if __has_attribute(__noclone__) || \
    (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 405
#define dontclone __attribute__((__noclone__))
#else
#define dontclone
#endif
#endif

#ifndef noinstrument
#if __has_attribute(__no_instrument_function__) || \
    (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 204
#define noinstrument __attribute__((__no_instrument_function__))
#else
#define noinstrument
#endif
#endif

#ifndef noasan
#if (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 408 || \
    __has_attribute(__no_sanitize_address__)
#define noasan __attribute__((__no_sanitize_address__))
#else
#define noasan
#endif
#endif

#ifndef notsan
#if (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 408 || \
    __has_attribute(__no_sanitize_thread__)
#define notsan __attribute__((__no_sanitize_thread__))
#else
#define notsan
#endif
#endif

#ifndef noubsan
#if (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 408 || \
    __has_attribute(__no_sanitize_undefined__)
#define noubsan __attribute__((__no_sanitize_undefined__))
#else
#define noubsan
#endif
#endif

#ifndef nomsan
#if __has_feature(memory_sanitizer)
#define __SANITIZE_MEMORY__
#define nomsan __attribute__((__no_sanitize__("memory")))
#else
#define nomsan
#endif
#endif

#ifndef smashmystack
#if __has_attribute(__no_stack_protector__)
#define smashmystack __attribute__((__no_stack_protector__))
#elif (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 407 || \
    __has_attribute(__optimize__)
#define smashmystack __attribute__((__optimize__("-fno-stack-protector")))
#else
#define smashmystack
#endif
#endif

#if LONG_BIT >= 64
#if (__GNUC__ + 0) * 100 + (__GNUC_MINOR__ + 0) >= 406 || defined(__llvm__)
#define HAVE_INT128
#endif
#endif

#if (defined(__x86_64__) || defined(__aarch64__)) &&                     \
    !defined(__SANITIZE_MEMORY__) && !defined(__SANITIZE_UNDEFINED__) && \
    !defined(__SANITIZE_THREAD__) && !defined(NOJIT)
#define HAVE_JIT
#endif

#if defined(HAVE_JIT) && defined(__GNUC__) && \
    !defined(__SANITIZE_ADDRESS__) && !defined(__SANITIZE_UNDEFINED__)
#ifndef __OPTIMIZE__
#define TRIVIALLY_RELOCATABLE \
  noinstrument dontclone noubsan smashmystack optimizesize
#else
#define TRIVIALLY_RELOCATABLE noinstrument dontclone noubsan smashmystack
#endif
#define MICRO_OP_SAFE TRIVIALLY_RELOCATABLE forceinline
#define MICRO_OP      TRIVIALLY_RELOCATABLE
#else
#define MICRO_OP_SAFE static inline
#define MICRO_OP
#endif

#endif /* BLINK_BUILTIN_H_ */
