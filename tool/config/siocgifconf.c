// checks if we can enumerate network interfaces
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

char buf[16384];

int main(int argc, char *argv[]) {
  int rc;
  struct ifreq *ifreq;
  struct ifconf ifconf;
  unsigned long magnums_we_care_about[] = {
      IFNAMSIZ,        //
      SIOCGIFADDR,     //
      SIOCGIFNETMASK,  //
      SIOCGIFBRDADDR,  //
      SIOCGIFDSTADDR,  //
  };
  ifconf.ifc_buf = buf;
  ifconf.ifc_len = sizeof(buf);
  socket(AF_INET, SOCK_DGRAM, 0);
  rc = ioctl(3, SIOCGIFCONF, &ifconf);
  if (rc) return 1;
  if (!ifconf.ifc_len) return 2;
  ifreq = ifconf.ifc_req;
  (void)ifreq;
  return 0;
}
