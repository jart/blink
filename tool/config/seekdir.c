// checks for telldir() and seekdir() support
#include <dirent.h>
#include <string.h>

int main(int argc, char *argv[]) {
  DIR *d;
  long spot;
  char name[256];
  struct dirent *e;
  if (!(d = opendir("/tmp"))) return 1;
  // read first entry, saving this position
  if ((spot = telldir(d)) == -1) return 2;
  if (!(e = readdir(d))) return 3;
  strcpy(name, e->d_name);
  // read second entry, ensuring it's different
  if (!(e = readdir(d))) return 4;
  if (!strcmp(name, e->d_name)) return 5;
  // read the first entry again, ensuring it's what we remember
  seekdir(d, spot);
  if (!(e = readdir(d))) return 6;
  if (strcmp(name, e->d_name)) return 7;
  return 0;
}
