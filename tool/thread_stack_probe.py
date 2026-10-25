#!/usr/bin/env python3
"""Check Linux libc's main-thread stack query (run using guest Python).

Example: BLINK_PREFIX=/path/to/debian/rootfs blink -jm /usr/bin/python3 \
    /SystemRoot/path/to/blink/tool/thread_stack_probe.py

Use a glibc guest: static musl does not exercise glibc's /proc/self/maps path.
"""
import ctypes
import os
import sys


def main():
    libc = ctypes.CDLL(None)
    libc.pthread_self.restype = ctypes.c_void_p
    libc.pthread_getattr_np.argtypes = [ctypes.c_void_p, ctypes.c_void_p]
    libc.pthread_attr_getstack.argtypes = [
        ctypes.c_void_p,
        ctypes.POINTER(ctypes.c_void_p),
        ctypes.POINTER(ctypes.c_size_t),
    ]
    libc.pthread_attr_destroy.argtypes = [ctypes.c_void_p]
    # Aligned storage larger than Linux x86-64 glibc's pthread_attr_t.
    attr = (ctypes.c_ulong * 32)()
    result = libc.pthread_getattr_np(libc.pthread_self(), attr)
    if result:
        print(f"pthread_getattr_np: {result} ({os.strerror(result)})")
        return 1
    try:
        base = ctypes.c_void_p()
        size = ctypes.c_size_t()
        result = libc.pthread_attr_getstack(attr, ctypes.byref(base), ctypes.byref(size))
        print(f"pthread_attr_getstack: {result}; base={hex(base.value or 0)}; size={size.value}")
        return int(bool(result or not base.value or not size.value))
    finally:
        libc.pthread_attr_destroy(attr)


if __name__ == "__main__":
    sys.exit(main())
