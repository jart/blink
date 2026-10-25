#define _GNU_SOURCE
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>
/* Each eventfd write wakes edge-triggered epoll, even without draining it. */
int main(void) {
 int ep=epoll_create1(0),fd=eventfd(0,EFD_NONBLOCK);
 struct epoll_event ev={.events=EPOLLIN|EPOLLET},got;
 uint64_t one=1,value;
 if(ep<0 || fd<0 || epoll_ctl(ep,EPOLL_CTL_ADD,fd,&ev))return 1;
 for(int i=0;i<3;++i){
  if(write(fd,&one,8)!=8)return 2;
  if(epoll_wait(ep,&got,1,100)!=1)return 3+i;
 }
 if(read(fd,&value,8)!=8 || value!=3)return 6;
 return 0;
}
