#ifndef EASYCKDINT_H_
#define EASYCKDINT_H_
#if defined(__has_include) && __has_include(<stdckdint.h>)
#include <stdckdint.h>
#else

#define __STDC_VERSION_STDCKDINT_H__ 202311L

#define ckd_add(res, x, y) __builtin_add_overflow((x), (y), (res))
#define ckd_sub(res, x, y) __builtin_sub_overflow((x), (y), (res))
#define ckd_mul(res, x, y) __builtin_mul_overflow((x), (y), (res))

#endif /* <stdckdint.h> */
#endif /* EASYCKDINT_H_ */
