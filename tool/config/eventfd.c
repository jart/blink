#include <sys/eventfd.h>
int main(void) { eventfd(0, -1); return 0; }
