// checks if F_GETOWN_EX will break build
#include <fcntl.h>

int main(int argc, char *argv[]) {
  unsigned long magnums[] = {
      F_GETOWN_EX,   //
      F_SETOWN_EX,   //
      F_OWNER_TID,   //
      F_OWNER_PID,   //
      F_OWNER_PGRP,  //
  };
  (void)magnums;
}
