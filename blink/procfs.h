#ifndef BLINK_PROCFS_H_
#define BLINK_PROCFS_H_

#include "blink/vfs.h"

extern struct VfsSystem g_procfs;

int ProcfsRegisterExe(i32 pid, const char *path);

#endif  // BLINK_PROCFS_H_
