#define _GNU_SOURCE
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
/* Linux MOD re-polls readiness, including unchanged edge-triggered interests. */
int main(void) {
 int p[2], ep; char byte;
 struct epoll_event event={.events=EPOLLIN|EPOLLET},got;
 if(socketpair(AF_UNIX,SOCK_STREAM,0,p)) return 1;
 if((ep=epoll_create1(0))<0) return 2;
 if(epoll_ctl(ep,EPOLL_CTL_ADD,p[0],&event)) return 3;
 if(write(p[1],"ab",2)!=2) return 4;
 if(epoll_wait(ep,&got,1,1000)!=1) return 5;
 if(read(p[0],&byte,1)!=1) return 6;
 if(epoll_wait(ep,&got,1,0)!=0) return 7;
 if(epoll_ctl(ep,EPOLL_CTL_MOD,p[0],&event)) return 8;
 if(epoll_wait(ep,&got,1,1000)!=1 || !(got.events&EPOLLIN)) return 9;
 event.events=EPOLLET;
 if(epoll_ctl(ep,EPOLL_CTL_MOD,p[0],&event)) return 10;
 event.events=EPOLLIN|EPOLLET;
 if(epoll_ctl(ep,EPOLL_CTL_MOD,p[0],&event)) return 11;
 if(epoll_wait(ep,&got,1,1000)!=1 || !(got.events&EPOLLIN)) return 12;
 close(p[0]);close(p[1]);close(ep);return 0;
}
