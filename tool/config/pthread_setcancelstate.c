// checks if pthread_setcancelstate is available and usable
#include <pthread.h>

void *Worker(void *arg) {
  return arg;
}

int main(int argc, char *argv[]) {
  if (pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, 0))
    return 1;
  if (pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0))
    return 1;
  return 0;
}
