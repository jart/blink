/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2023 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/un.h>
#include <unistd.h>

#include "blink/ancillary.h"
#include "blink/assert.h"
#include "blink/endian.h"
#include "blink/errno.h"
#include "blink/fds.h"
#include "blink/linux.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/util.h"

/**
 * @fileoverview Ancillary Socket Data Marshalling
 *
 * This module marshals `msg_control` for sendmsg() and recvmsg(), which
 * is used by UNIX domain sockets to perform tricks such as sharing file
 * descriptors between processes and X11 server authentication.
 */

static int AppendCmsg(struct Machine *m, struct msghdr *msg, int level,
                      int type, const void *data, size_t size) {
  struct cmsghdr *cmsg;
  if (!msg->msg_control &&
      !(msg->msg_control = AddToFreeList(m, calloc(1, kMaxAncillary)))) {
    return -1;
  }
  if (msg->msg_controllen + CMSG_SPACE(size) > kMaxAncillary) {
    LOGF("kMaxAncillary exceeded");
    return enomem();
  }
  cmsg = (struct cmsghdr *)((u8 *)msg->msg_control + msg->msg_controllen);
  cmsg->cmsg_len = CMSG_LEN(size);
  cmsg->cmsg_level = level;
  cmsg->cmsg_type = type;
  memcpy(CMSG_DATA(cmsg), data, size);
  msg->msg_controllen += CMSG_SPACE(size);
  return 0;
}

#ifdef SCM_CREDENTIALS
static int SendScmCredentials(struct Machine *m, struct msghdr *msg,
                              const struct ucred_linux *payload,
                              size_t elements) {
  struct ucred ucred;
  if (elements != 1) return einval();
  ucred.pid = Read32(payload->pid);
  ucred.uid = Read32(payload->uid);
  ucred.gid = Read32(payload->gid);
  SYS_LOGF("SendScmCredentials(pid=%d, uid=%d, gid=%d)", ucred.pid, ucred.uid,
           ucred.gid);
  return AppendCmsg(m, msg, SOL_SOCKET, SCM_CREDENTIALS, &ucred, sizeof(ucred));
}
#endif

#ifdef SCM_RIGHTS
static int SendScmRights(struct Machine *m, struct msghdr *msg,
                         const u8 *payload, size_t elements) {
  size_t i;
  int fd[SCM_MAX_FD_LINUX];
  if (elements > SCM_MAX_FD_LINUX) {
    LOGF("too many scm_rights fds");
    return einval();
  }
  for (i = 0; i < elements; ++i) {
    fd[i] = Read32(payload + i * 4);
    SYS_LOGF("SendScmRights(fd=%d)", fd[i]);
  }
  return AppendCmsg(m, msg, SOL_SOCKET, SCM_RIGHTS, &fd, i * sizeof(int));
}
#endif

static ssize_t GetAncillaryElementLength(const struct cmsghdr_linux *gcmsg) {
  switch (Read32(gcmsg->level)) {
    case SOL_SOCKET_LINUX:
      switch (Read32(gcmsg->type)) {
#ifdef SCM_RIGHTS
        case SCM_RIGHTS_LINUX:
          return 4;
#endif
#ifdef SCM_CREDENTIALS
        case SCM_CREDENTIALS_LINUX:
          return sizeof(struct ucred_linux);
#endif
        default:
          break;
      }
    default:
      break;
  }
  LOGF("%s ancillary level=%d type=%d", "unsupported", Read32(gcmsg->level),
       Read32(gcmsg->type));
  return einval();
}

int SendAncillary(struct Machine *m, struct msghdr *msg,
                  const struct msghdr_linux *gm) {
  ssize_t rc;
  u32 len, need;
  void *payload;
  size_t avail, i, space, elements;
  const struct cmsghdr_linux *gcmsg;
  for (i = 0; (avail = Read64(gm->controllen) - i); i += space) {
    if (sizeof(*gcmsg) > avail) {
      LOGF("ancillary corrupted");
      return einval();
    }
    if (!(gcmsg = (const struct cmsghdr_linux *)SchlepR(
              m, Read64(gm->control) + i, sizeof(*gcmsg)))) {
      return -1;
    }
    len = Read32(gcmsg->len);
    if (len > ROUNDUP(sizeof(*gcmsg), 8)) {
      len -= ROUNDUP(sizeof(*gcmsg), 8);
    } else {
      len = 0;
    }
    space = ROUNDUP(sizeof(*gcmsg), 8) + ROUNDUP(len, 8);
    if (space > avail) {
    ThisCorruption:
      LOGF("ancillary corrupted len=%u level=%u type=%u avail=%zu space=%zu",
           len, Read32(gcmsg->level), Read32(gcmsg->type), avail, space);
      return einval();
    }
    if ((rc = GetAncillaryElementLength(gcmsg)) == -1) {
      return -1;
    }
    if (len) {
      if (!(payload =
                SchlepR(m, Read64(gm->control) + i + ROUNDUP(sizeof(*gcmsg), 8),
                        ROUNDUP(len, 8)))) {
        return -1;
      }
      if ((need = rc) && need <= len) {
        elements = len / need;
        if (len % need != 0) {
          goto ThisCorruption;
        }
      } else {
        goto ThisCorruption;
      }
    } else {
      payload = 0;
      elements = 0;
    }
    switch (Read32(gcmsg->level)) {
      case SOL_SOCKET_LINUX:
        switch (Read32(gcmsg->type)) {
#ifdef SCM_RIGHTS
          case SCM_RIGHTS_LINUX:
            if (SendScmRights(m, msg, (const u8 *)payload, elements) == -1)
              return -1;
            break;
#endif
#ifdef SCM_CREDENTIALS
          case SCM_CREDENTIALS_LINUX:
            if (SendScmCredentials(m, msg, (const struct ucred_linux *)payload,
                                   elements) == -1)
              return -1;
            break;
#endif
          default:
            unassert(!"inconsistent ancillary type");
            __builtin_unreachable();
        }
        break;
      default:
        unassert(!"inconsistent ancillary level");
        __builtin_unreachable();
    }
  }
  return 0;
}

static i64 CopyCmsg(struct Machine *m, struct msghdr_linux *gm, int level,
                    int type, void *data, size_t len, u64 i) {
  i64 control;
  size_t hdrspace;
  struct cmsghdr_linux gcmsg;
  hdrspace = ROUNDUP(sizeof(gcmsg), 8);
  Write64(gcmsg.len, hdrspace + len);
  Write32(gcmsg.level, level);
  Write32(gcmsg.type, type);
  control = Read64(gm->control);
  unassert(CopyToUserWrite(m, control + i, &gcmsg, sizeof(gcmsg)) != -1);
  unassert(CopyToUserWrite(m, control + i + hdrspace, data, len) != -1);
  return hdrspace + ROUNDUP(len, 8);
}

static void TrackScmRightsFd(struct Machine *m, int fildes, int flags) {
  int oflags;
  struct Fd *fd;
  SYS_LOGF("ReceiveScmRights(fd=%d)", fildes);
  unassert((oflags = fcntl(fildes, F_GETFL, 0)) != -1);
  fd =
      AddFd(&m->system->fds, fildes,
            oflags | O_RDWR | (flags & MSG_CMSG_CLOEXEC_LINUX ? O_CLOEXEC : 0));
  if (!GetFdSocketType(fildes, &fd->socktype)) {
    fd->norestart = IsNoRestartSocket(fildes);
  }
#ifndef MSG_CMSG_CLOEXEC
  if (flags & MSG_CMSG_CLOEXEC_LINUX) {
    unassert(!fcntl(fildes, F_SETFD, FD_CLOEXEC));
  }
#endif
}

#ifdef SCM_RIGHTS
static i64 ReceiveScmRights(struct Machine *m, struct msghdr_linux *gm,
                            struct cmsghdr *cmsg, u64 offset, int flags) {
  const int *p, *e;
  size_t space, avail;
  size_t i, received, relayable;
  u8 fd[SCM_MAX_FD_LINUX][4];
  // determine how many file descriptors we received from the host
  p = (const int *)CMSG_DATA(cmsg);
  e = (const int *)((const u8 *)cmsg + cmsg->cmsg_len);
  received = e - p;
  unassert(received > 0);
  avail = Read64(gm->controllen) - offset;
  space = ROUNDUP(sizeof(struct cmsghdr_linux), 8);
  // determine how many file descriptors we can relay to the guest
  if (space + 4 < avail) {
    relayable = MIN(MIN(received, (avail - space) / 4), SCM_MAX_FD_LINUX);
    if (relayable != received) {
      LOGF("truncated %zd scm_rights fds", received - relayable);
      Write32(gm->flags, Read32(gm->flags) | MSG_CTRUNC_LINUX);
    }
    // serialize fds
    for (i = 0; i < relayable; ++i) {
      Write32(fd[i], p[i]);
      TrackScmRightsFd(m, p[i], flags);
    }
    // close excess fds
    for (i = relayable; i < received; ++i) {
      close(p[i]);
    }
    return CopyCmsg(m, gm, SOL_SOCKET_LINUX, SCM_RIGHTS_LINUX, fd,
                    relayable * 4, offset);
  } else {
    // truncate the control message because there wasn't enough
    // room in the guest's memory for a single file descriptor.
    for (i = 0; i < received; ++i) {
      close(p[i]);
    }
    return 0;
  }
}
#endif

#ifdef SCM_CREDENTIALS
static i64 ReceiveScmCredentials(struct Machine *m, struct msghdr_linux *gm,
                                 struct cmsghdr *cmsg, u64 offset) {
  struct ucred_linux gucred;
  const struct ucred *ucred;
  ucred = (const struct ucred *)CMSG_DATA(cmsg);
  SYS_LOGF("ReceiveScmCredentials(pid=%d, uid=%d, gid=%d)", ucred->pid,
           ucred->uid, ucred->gid);
  if (offset + ROUNDUP(sizeof(struct cmsghdr_linux), 8) +
          ROUNDUP(sizeof(gucred), 8) <=
      Read64(gm->controllen)) {
    Write32(gucred.pid, ucred->pid);
    Write32(gucred.uid, ucred->uid);
    Write32(gucred.gid, ucred->gid);
    return CopyCmsg(m, gm, SOL_SOCKET_LINUX, SCM_CREDENTIALS_LINUX, &gucred,
                    sizeof(gucred), offset);
  } else {
    return 0;
  }
}
#endif

static i64 ReceiveControlMessage(struct Machine *m, struct msghdr_linux *gm,
                                 struct cmsghdr *cmsg, u64 offset, int flags) {
  if (cmsg->cmsg_level == SOL_SOCKET) {
#ifdef SCM_RIGHTS
    if (cmsg->cmsg_type == SCM_RIGHTS) {
      return ReceiveScmRights(m, gm, cmsg, offset, flags);
    }
#endif
#ifdef SCM_CREDENTIALS
    if (cmsg->cmsg_type == SCM_CREDENTIALS) {
      return ReceiveScmCredentials(m, gm, cmsg, offset);
    }
#endif
  }
  LOGF("%s ancillary level=%d type=%d", "unsupported", cmsg->cmsg_level,
       cmsg->cmsg_type);
  return enotsup();
}

int ReceiveAncillary(struct Machine *m, struct msghdr_linux *gm,
                     struct msghdr *msg, int flags) {
  ssize_t rc;
  u64 offset = 0;
  struct cmsghdr *cmsg;
  if ((cmsg = CMSG_FIRSTHDR(msg))) {
    do {
      switch ((rc = ReceiveControlMessage(m, gm, cmsg, offset, flags))) {
        case -1:
          return -1;
        case 0:
          LOGF("%s ancillary level=%d type=%d", "truncated", cmsg->cmsg_level,
               cmsg->cmsg_type);
          Write32(gm->flags, Read32(gm->flags) | MSG_CTRUNC_LINUX);
          offset = Read64(gm->controllen);
          break;
        default:
          offset += rc;
          break;
      }
    } while ((cmsg = CMSG_NXTHDR(msg, cmsg)));
  }
  Write64(gm->controllen, offset);
  return 0;
}
