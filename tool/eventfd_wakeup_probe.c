#define _GNU_SOURCE
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <time.h>
#include <unistd.h>
static int fd;
static void *sender(void *unused) { uint64_t one=1;usleep(20000);if(write(fd,&one,8)!=8)_exit(3);return 0; }
int main(void) {
 int ep=epoll_create1(0);pthread_t thread;struct timespec a,b;
 struct epoll_event ev={.events=EPOLLIN|EPOLLET},got;uint64_t value;
 if(ep<0 || (fd=eventfd(0,EFD_NONBLOCK))<0)return 1;
 if(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&ev))return 2;
 clock_gettime(CLOCK_MONOTONIC,&a);
 if(pthread_create(&thread,0,sender,0))return 4;
 int n=epoll_wait(ep,&got,1,1000);
 clock_gettime(CLOCK_MONOTONIC,&b);
 double elapsed=(b.tv_sec-a.tv_sec)+(b.tv_nsec-a.tv_nsec)/1e9;
 printf("events=%d elapsed=%.6f\n",n,elapsed);
 pthread_join(thread,0);
 if(n!=1)return 5;
 if(read(fd,&value,8)!=8 || value!=1)return 6;
 return elapsed>0.5 ? 7 : 0;
}
