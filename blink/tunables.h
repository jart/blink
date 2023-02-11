#ifndef BLINK_TUNABLES_H_
#define BLINK_TUNABLES_H_
#include "blink/builtin.h"

#if CAN_64BIT
#ifdef __APPLE__
#define kSkew 0x088800000000
#else
#define kSkew 0x000000000000
#endif
#define kAutomapStart  0x200000000000
#define kAutomapEnd    0x400000000000
#define kPreciousStart 0x444000000000  // 1 tb
#define kPreciousEnd   0x454000000000
#define kDynInterpAddr 0x454000000000
#define kDynExecAddr   0x455000000000
#define kStackTop      0x500000000000
#else
#define kAutomapStart  0x20000000
#define kAutomapEnd    0x70000000
#define kSkew          0x00000000
#define kStackTop      0xf8000000
#define kPreciousStart 0x44000000  // 192 mb
#define kPreciousEnd   0x50000000
#define kDynInterpAddr 0x40000000
#define kDynExecAddr   0x41000000
#endif

#define kRealSize  (16 * 1024 * 1024)  // size of ram for real mode
#define kStackSize (8 * 1024 * 1024)   // size of stack for user mode
#define kMinBrk    (2 * 1024 * 1024)   // minimum user mode image address

#define kMinBlinkFd   123       // fds owned by the vm start here
#define kPollingMs    50        // busy loop for futex(), poll(), etc.
#define kSemSize      128       // number of bytes used for each semaphore
#define kBusCount     256       // # load balanced semaphores in virtual bus
#define kBusRegion    kSemSize  // 16 is sufficient for 8-byte loads/stores
#define kFutexMax     100
#define kRedzoneSize  128
#define kMaxMapSize   (8ULL * 1024 * 1024 * 1024)
#define kMaxResident  (8ULL * 1024 * 1024 * 1024)
#define kMaxVirtual   (kMaxResident * 8)
#define kMaxAncillary 1000
#define kMaxSigDepth  8

#endif /* BLINK_TUNABLES_H_ */
