#ifndef TSAN_H_
#define TSAN_H_
// Thread Safety Analysis, complements of Google

#if defined(__cplusplus) && defined(__clang__)
#define THREAD_ANNOTATION_ATTRIBUTE__(x) __attribute__((x))
#else
#define THREAD_ANNOTATION_ATTRIBUTE__(x)
#endif

// GUARDED_BY()
//
// Documents if a shared field or global variable needs to be protected by a
// mutex. GUARDED_BY() allows the user to specify a particular mutex that
// should be held when accessing the annotated variable.
//
// Example:
//
//   Mutex mu;
//   int p1 GUARDED_BY(mu);
#define GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(guarded_by(x))

// PT_GUARDED_BY()
//
// Documents if the memory location pointed to by a pointer should be guarded
// by a mutex when dereferencing the pointer.
//
// Example:
//   Mutex mu;
//   int *p1 PT_GUARDED_BY(mu);
//
// Note that a pointer variable to a shared memory location could itself be a
// shared variable.
//
// Example:
//
//     // `q`, guarded by `mu1`, points to a shared memory location that is
//     // guarded by `mu2`:
//     int *q GUARDED_BY(mu1) PT_GUARDED_BY(mu2);
#define PT_GUARDED_BY(x) THREAD_ANNOTATION_ATTRIBUTE__(pt_guarded_by(x))

// ACQUIRED_AFTER() / ACQUIRED_BEFORE()
//
// Documents the acquisition order between locks that can be held
// simultaneously by a thread. For any two locks that need to be annotated
// to establish an acquisition order, only one of them needs the annotation.
// (i.e. You don't have to annotate both locks with both ACQUIRED_AFTER
// and ACQUIRED_BEFORE.)
//
// Example:
//
//   Mutex m1;
//   Mutex m2 ACQUIRED_AFTER(m1);
#define ACQUIRED_AFTER(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(acquired_after(__VA_ARGS__))
#define ACQUIRED_BEFORE(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(acquired_before(__VA_ARGS__))

// EXCLUSIVE_LOCKS_REQUIRED() / SHARED_LOCKS_REQUIRED()
//
// Documents a function that expects a mutex to be held prior to entry.
// The mutex is expected to be held both on entry to, and exit from, the
// function.
//
// Example:
//
//   Mutex mu1, mu2;
//   int a GUARDED_BY(mu1);
//   int b GUARDED_BY(mu2);
//
//   void foo() EXCLUSIVE_LOCKS_REQUIRED(mu1, mu2) { ... };
#define EXCLUSIVE_LOCKS_REQUIRED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(exclusive_locks_required(__VA_ARGS__))
#define SHARED_LOCKS_REQUIRED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(shared_locks_required(__VA_ARGS__))

// LOCKS_EXCLUDED()
//
// Documents the locks acquired in the body of the function. These locks
// cannot be held when calling this function (as Abseil's `Mutex` locks are
// non-reentrant).
#define LOCKS_EXCLUDED(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(locks_excluded(__VA_ARGS__))

// LOCK_RETURNED()
//
// Documents a function that returns a mutex without acquiring it.  For example,
// a public getter method that returns a pointer to a private mutex should
// be annotated with LOCK_RETURNED.
#define LOCK_RETURNED(x) THREAD_ANNOTATION_ATTRIBUTE__(lock_returned(x))

// LOCKABLE
//
// Documents if a class/type is a lockable type (such as the `Mutex` class).
#define LOCKABLE THREAD_ANNOTATION_ATTRIBUTE__(lockable)

// SCOPED_LOCKABLE
//
// Documents if a class does RAII locking (such as the `MutexLock` class).
// The constructor should use `LOCK_FUNCTION()` to specify the mutex that is
// acquired, and the destructor should use `UNLOCK_FUNCTION()` with no
// arguments; the analysis will assume that the destructor unlocks whatever the
// constructor locked.
#define SCOPED_LOCKABLE THREAD_ANNOTATION_ATTRIBUTE__(scoped_lockable)

// EXCLUSIVE_LOCK_FUNCTION()
//
// Documents functions that acquire a lock in the body of a function, and do
// not release it.
#define EXCLUSIVE_LOCK_FUNCTION(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(exclusive_lock_function(__VA_ARGS__))

// SHARED_LOCK_FUNCTION()
//
// Documents functions that acquire a shared (reader) lock in the body of a
// function, and do not release it.
#define SHARED_LOCK_FUNCTION(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(shared_lock_function(__VA_ARGS__))

// UNLOCK_FUNCTION()
//
// Documents functions that expect a lock to be held on entry to the function,
// and release it in the body of the function.
#define UNLOCK_FUNCTION(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(unlock_function(__VA_ARGS__))

// EXCLUSIVE_TRYLOCK_FUNCTION() / SHARED_TRYLOCK_FUNCTION()
//
// Documents functions that try to acquire a lock, and return success or failure
// (or a non-boolean value that can be interpreted as a boolean).
// The first argument should be `true` for functions that return `true` on
// success, or `false` for functions that return `false` on success. The second
// argument specifies the mutex that is locked on success. If unspecified, this
// mutex is assumed to be `this`.
#define EXCLUSIVE_TRYLOCK_FUNCTION(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(exclusive_trylock_function(__VA_ARGS__))
#define SHARED_TRYLOCK_FUNCTION(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(shared_trylock_function(__VA_ARGS__))

// ASSERT_EXCLUSIVE_LOCK() / ASSERT_SHARED_LOCK()
//
// Documents functions that dynamically check to see if a lock is held, and fail
// if it is not held.
#define ASSERT_EXCLUSIVE_LOCK(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(assert_exclusive_lock(__VA_ARGS__))
#define ASSERT_SHARED_LOCK(...) \
  THREAD_ANNOTATION_ATTRIBUTE__(assert_shared_lock(__VA_ARGS__))

// NO_THREAD_SAFETY_ANALYSIS
//
// Turns off thread safety checking within the body of a particular function.
// This annotation is used to mark functions that are known to be correct, but
// the locking behavior is more complicated than the analyzer can handle.
#define NO_THREAD_SAFETY_ANALYSIS \
  THREAD_ANNOTATION_ATTRIBUTE__(no_thread_safety_analysis)

#if defined(__has_feature) && !defined(__SANITIZE_THREAD__)
#if __has_feature(thread_sanitizer)
#define __SANITIZE_THREAD__ 1
#endif
#endif
#if defined(__SANITIZE_THREAD__)
#ifdef __cplusplus
extern "C" {
#endif
void AnnotateIgnoreWritesBegin(const char *file, int line);
void AnnotateIgnoreWritesEnd(const char *file, int line);
void AnnotateIgnoreReadsBegin(const char *file, int line);
void AnnotateIgnoreReadsEnd(const char *file, int line);
#ifdef __cplusplus
}
#endif
#define IGNORE_RACES_START()                       \
  do {                                             \
    AnnotateIgnoreReadsBegin(__FILE__, __LINE__);  \
    AnnotateIgnoreWritesBegin(__FILE__, __LINE__); \
  } while (0)
#define IGNORE_RACES_END()                       \
  do {                                           \
    AnnotateIgnoreWritesEnd(__FILE__, __LINE__); \
    AnnotateIgnoreReadsEnd(__FILE__, __LINE__);  \
  } while (0)
#else
#define IGNORE_RACES_START()
#define IGNORE_RACES_END()
#endif

#endif /* TSAN_H_ */
