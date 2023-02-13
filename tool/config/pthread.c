// checks posix threads works
#include <pthread.h>

void *Worker(void *arg) {
  return arg;
}

int main(int argc, char *argv[]) {
  pthread_t th;
  void *arg, *res;
  arg = (void *)argv;
  if (pthread_create(&th, 0, Worker, arg)) return 1;
  if (pthread_join(th, &res)) return 2;
  if (res != arg) return 3;
  return 0;
}
