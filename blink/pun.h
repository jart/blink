#ifndef BLINK_PUN_H_
#define BLINK_PUN_H_
#include <stdint.h>

union FloatPun {
  float f;
  uint32_t i;
};

union DoublePun {
  double f;
  uint64_t i;
};

union FloatVectorPun {
  union FloatPun u[4];
  float f[4];
};

union DoubleVectorPun {
  union DoublePun u[2];
  double f[2];
};

#endif /* BLINK_PUN_H_ */
