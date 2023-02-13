// checks setgroups() won't break build
#include <grp.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  (void)setgroups(0, 0);
  return 0;
}
