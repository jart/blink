#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/tcp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
int main(int argc,char **argv) {
 int server=socket(AF_INET,SOCK_STREAM,0), sock, peer,ep,status,one=1,zero=0;
 struct sockaddr_in addr={.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};
 socklen_t len=sizeof(addr);size_t total=0;const size_t size=16*1024*1024;
 static char buf[512*1024];struct timespec start,end;
 if(server<0 || bind(server,(void*)&addr,len) || getsockname(server,(void*)&addr,&len) || listen(server,1)) return 1;
 pid_t child=fork();if(child<0)return 2;
 if(!child){peer=accept(server,0,0);if(peer<0)_exit(3);while(total<size){ssize_t n=write(peer,buf,sizeof(buf));if(n<=0)_exit(4);total+=n;}close(peer);_exit(0);}
 sock=socket(AF_INET,SOCK_STREAM,0);if(connect(sock,(void*)&addr,len))return 5;
 ep=epoll_create1(0);struct epoll_event event={.events=EPOLLIN|EPOLLET},got;
 if(ep<0 || epoll_ctl(ep,EPOLL_CTL_ADD,sock,&event))return 6;
 clock_gettime(CLOCK_MONOTONIC,&start);
 while(total<size){
  if(epoll_wait(ep,&got,1,5000)!=1)return 7;
  if(argc>1 && setsockopt(sock,IPPROTO_TCP,TCP_CORK,&one,sizeof(one)))return 8;
  ssize_t n=recv(sock,buf,sizeof(buf),MSG_DONTWAIT);
  if(n>0)total+=n;else if(n==0 || (errno!=EAGAIN&&errno!=EWOULDBLOCK))return 9;
  if(argc>1 && setsockopt(sock,IPPROTO_TCP,TCP_CORK,&zero,sizeof(zero)))return 10;
  if(epoll_ctl(ep,EPOLL_CTL_MOD,sock,&event))return 11;
 }
 clock_gettime(CLOCK_MONOTONIC,&end);
 close(sock);close(server);close(ep);
 if(waitpid(child,&status,0)!=child || !WIFEXITED(status) || WEXITSTATUS(status))return 12;
 printf("bytes=%zu seconds=%.6f cork=%d\n",total,(end.tv_sec-start.tv_sec)+(end.tv_nsec-start.tv_nsec)/1e9,argc>1);return 0;
}
