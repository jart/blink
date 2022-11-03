#ifndef BLINK_ENDIAN_H_
#define BLINK_ENDIAN_H_
#include <stdint.h>
#if !defined(__GNUC__) || __SIZEOF_LONG__ < 8

static inline uint8_t Read8(const uint8_t *p) {
  return p[0];
}

static inline uint16_t Read16(const uint8_t *p) {
  return p[1] << 8 | p[0];
}

static inline void Write8(uint8_t *p, uint8_t v) {
  *p = v;
}

static inline void Write16(uint8_t *p, uint16_t v) {
  p[0] = (0x00FF & v) >> 000;
  p[1] = (0xFF00 & v) >> 010;
}

static inline uint32_t Read32(const uint8_t *p) {
  return ((uint32_t)p[0] << 000 | (uint32_t)p[1] << 010 |
          (uint32_t)p[2] << 020 | (uint32_t)p[3] << 030);
}

static inline uint64_t Read64(const uint8_t *p) {
  return ((uint64_t)p[0] << 000 | (uint64_t)p[1] << 010 |
          (uint64_t)p[2] << 020 | (uint64_t)p[3] << 030 |
          (uint64_t)p[4] << 040 | (uint64_t)p[5] << 050 |
          (uint64_t)p[6] << 060 | (uint64_t)p[7] << 070);
}

static inline void Write32(uint8_t *p, uint32_t v) {
  p[0] = (0x000000FF & v) >> 000;
  p[1] = (0x0000FF00 & v) >> 010;
  p[2] = (0x00FF0000 & v) >> 020;
  p[3] = (0xFF000000 & v) >> 030;
}

static inline void Write64(uint8_t *p, uint64_t v) {
  p[0] = (0x00000000000000FF & v) >> 000;
  p[1] = (0x000000000000FF00 & v) >> 010;
  p[2] = (0x0000000000FF0000 & v) >> 020;
  p[3] = (0x00000000FF000000 & v) >> 030;
  p[4] = (0x000000FF00000000 & v) >> 040;
  p[5] = (0x0000FF0000000000 & v) >> 050;
  p[6] = (0x00FF000000000000 & v) >> 060;
  p[7] = (0xFF00000000000000 & v) >> 070;
}

#else

#define Read8(P) (*((const uint8_t *)(P)))

#define Read16(P)             \
  ({                          \
    const uint8_t *Ptr = (P); \
    Ptr[1] << 8 | Ptr[0];     \
  })

#define Read32(P)                                        \
  ({                                                     \
    const uint8_t *Ptr = (P);                            \
    ((uint32_t)Ptr[0] << 000 | (uint32_t)Ptr[1] << 010 | \
     (uint32_t)Ptr[2] << 020 | (uint32_t)Ptr[3] << 030); \
  })

#define Read64(P)                                        \
  ({                                                     \
    const uint8_t *Ptr = (P);                            \
    ((uint64_t)Ptr[0] << 000 | (uint64_t)Ptr[1] << 010 | \
     (uint64_t)Ptr[2] << 020 | (uint64_t)Ptr[3] << 030 | \
     (uint64_t)Ptr[4] << 040 | (uint64_t)Ptr[5] << 050 | \
     (uint64_t)Ptr[6] << 060 | (uint64_t)Ptr[7] << 070); \
  })

#define Write8(P, V)    \
  do {                  \
    uint8_t Val = (V);  \
    uint8_t *Ptr = (P); \
    *Ptr = Val;         \
  } while (0)

#define Write16(P, V)                           \
  do {                                          \
    int Val = (V);                              \
    uint8_t *Ptr = (P);                         \
    Ptr[0] = (0x00000000000000FF & Val) >> 000; \
    Ptr[1] = (0x000000000000FF00 & Val) >> 010; \
  } while (0)

#define Write32(P, V)                           \
  do {                                          \
    uint32_t Val = (V);                         \
    uint8_t *Ptr = (P);                         \
    Ptr[0] = (0x00000000000000FF & Val) >> 000; \
    Ptr[1] = (0x000000000000FF00 & Val) >> 010; \
    Ptr[2] = (0x0000000000FF0000 & Val) >> 020; \
    Ptr[3] = (0x00000000FF000000 & Val) >> 030; \
  } while (0)

#define Write64(P, V)                           \
  do {                                          \
    uint64_t Val = (V);                         \
    uint8_t *Ptr = (P);                         \
    Ptr[0] = (0x00000000000000FF & Val) >> 000; \
    Ptr[1] = (0x000000000000FF00 & Val) >> 010; \
    Ptr[2] = (0x0000000000FF0000 & Val) >> 020; \
    Ptr[3] = (0x00000000FF000000 & Val) >> 030; \
    Ptr[4] = (0x000000FF00000000 & Val) >> 040; \
    Ptr[5] = (0x0000FF0000000000 & Val) >> 050; \
    Ptr[6] = (0x00FF000000000000 & Val) >> 060; \
    Ptr[7] = (0xFF00000000000000 & Val) >> 070; \
  } while (0)

#endif /* GNU64 */
#endif /* BLINK_ENDIAN_H_ */
