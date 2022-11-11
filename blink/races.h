#ifndef RACES_H_
#define RACES_H_

#if defined(__has_feature) && !defined(__SANITIZE_THREAD__)
#if __has_feature(thread_sanitizer)
#define __SANITIZE_THREAD__ 1
#endif
#endif
#if defined(__SANITIZE_THREAD__)
void AnnotateIgnoreWritesBegin(const char *file, int line);
void AnnotateIgnoreWritesEnd(const char *file, int line);
void AnnotateIgnoreReadsBegin(const char *file, int line);
void AnnotateIgnoreReadsEnd(const char *file, int line);
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

#endif /* RACES_H_ */
