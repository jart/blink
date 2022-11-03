#ifndef BLINK_CASE_H_
#define BLINK_CASE_H_

#define CASE(OP, CODE) \
  case OP:             \
    CODE;              \
    break

#define CASR(OP, CODE) \
  case OP:             \
    CODE;              \
    return

#define XLAT(x, y) \
  case x:          \
    return y

#endif /* BLINK_CASE_H_ */
