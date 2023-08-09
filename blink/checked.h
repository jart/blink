#ifndef EASYCKDINT_H_
#define EASYCKDINT_H_

#ifdef __has_include
#if __has_include(<stdckdint.h>)
#include <stdckdint.h>
#ifndef ckd_add
#error "<stdckdint.h> lacks ckd_add() macro?"
#endif /* !ckd_add */
#endif /* <stdckdint.h> */
#endif /* __has_include */

#ifndef ckd_add

#define __STDC_VERSION_STDCKDINT_H__ 202311L

#if defined(__GNUC__) && __GNUC__ >= 5 && !defined(__ICC)
#define ckd_add(res, x, y) __builtin_add_overflow((x), (y), (res))
#define ckd_sub(res, x, y) __builtin_sub_overflow((x), (y), (res))
#define ckd_mul(res, x, y) __builtin_mul_overflow((x), (y), (res))
#endif /* __GNUC__ >= 5 && !__ICC */

#if !defined(ckd_add) && defined(__has_builtin)
#if __has_builtin(__builtin_add_overflow) && \
    __has_builtin(__builtin_sub_overflow) && \
    __has_builtin(__builtin_mul_overflow)
#define ckd_add(res, x, y) __builtin_add_overflow((x), (y), (res))
#define ckd_sub(res, x, y) __builtin_sub_overflow((x), (y), (res))
#define ckd_mul(res, x, y) __builtin_mul_overflow((x), (y), (res))
#endif /* __builtin_..._overflow */
#endif /* !ckd_add && __has_builtin */

#ifndef ckd_add /* FIXME */
#define ckd_add(res, x, y) (*(res) = (x) + (y), 0)
#define ckd_sub(res, x, y) (*(res) = (x) - (y), 0)
#define ckd_mul(res, x, y) (*(res) = (x) * (y), 0)
#endif /* !ckd_add */

#endif /* !ckd_add */

#endif /* EASYCKDINT_H_ */
