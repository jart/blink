#ifndef BLINK_PUN_H_
#define BLINK_PUN_H_
#include "blink/types.h"

union FloatPun {
  float f;
  u32 i;
};

union DoublePun {
  double f;
  u64 i;
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
