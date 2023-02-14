// checks for __int128 support

int main(int argc, char *argv[]) {
  volatile __int128 x;
  x = 0;
  return 0;
}
