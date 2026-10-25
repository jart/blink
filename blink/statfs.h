#ifndef BLINK_STATFS_H_
#define BLINK_STATFS_H_
#include <sys/types.h>
#include <sys/statvfs.h>
#include "blink/macros.h"
#if (defined(__linux) || defined(__APPLE__) || defined(__FreeBSD__) || \
     defined(__OpenBSD__)) && !defined(_POSIX_C_SOURCE)
#ifdef __linux
#include <sys/vfs.h>
#else
#include <sys/param.h>
#include <sys/mount.h>
#endif
typedef struct statfs HostStatfs;
#define HostStatfsPath statfs
#define HostStatfsFd fstatfs
#else
typedef struct statvfs HostStatfs;
#define HostStatfsPath statvfs
#define HostStatfsFd fstatvfs
#endif
#endif
