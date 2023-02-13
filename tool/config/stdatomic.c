// checks for c11 atomics
#include <stdatomic.h>

atomic_int x;

int main(int argc, char *argv[]) {
  if (atomic_exchange_explicit(&x, 123, memory_order_relaxed) != 0) return 1;
  if (atomic_exchange_explicit(&x, 456, memory_order_relaxed) != 123) return 2;
  return 0;
}
