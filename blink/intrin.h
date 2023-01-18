#ifndef BLINK_INTRIN_H_
#define BLINK_INTRIN_H_

#if defined(__x86_64__) && defined(__GNUC__) && __GNUC__ >= 6
#define X86_INTRINSICS 1
typedef char char_xmmu_t __attribute__((__vector_size__(16), __may_alias__));
typedef char char_xmma_t
    __attribute__((__vector_size__(16), __aligned__(16), __may_alias__));
#else
#define X86_INTRINSICS 0
#endif

#endif /* BLINK_INTRIN_H_ */
