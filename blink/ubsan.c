/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
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
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#define kUbsanKindInt     0
#define kUbsanKindFloat   1
#define kUbsanKindUnknown 0xffff

typedef void __ubsan_die_f(void);

struct UbsanSourceLocation {
  const char *file;
  uint32_t line;
  uint32_t column;
};

struct UbsanTypeDescriptor {
  uint16_t kind; /* int,float,... */
  uint16_t info; /* if int bit 0 if signed, remaining bits are log2(sizeof*8) */
  char name[];
};

struct UbsanTypeMismatchInfo {
  struct UbsanSourceLocation location;
  struct UbsanTypeDescriptor *type;
  uintptr_t alignment;
  uint8_t type_check_kind;
};

struct UbsanTypeMismatchInfoClang {
  struct UbsanSourceLocation location;
  struct UbsanTypeDescriptor *type;
  unsigned char log_alignment; /* https://reviews.llvm.org/D28244 */
  uint8_t type_check_kind;
};

struct UbsanOutOfBoundsInfo {
  struct UbsanSourceLocation location;
  struct UbsanTypeDescriptor *array_type;
  struct UbsanTypeDescriptor *index_type;
};

struct UbsanUnreachableData {
  struct UbsanSourceLocation location;
};

struct UbsanVlaBoundData {
  struct UbsanSourceLocation location;
  struct UbsanTypeDescriptor *type;
};

struct UbsanNonnullArgData {
  struct UbsanSourceLocation location;
  struct UbsanSourceLocation attr_location;
};

struct UbsanCfiBadIcallData {
  struct UbsanSourceLocation location;
  struct UbsanTypeDescriptor *type;
};

struct UbsanNonnullReturnData {
  struct UbsanSourceLocation location;
  struct UbsanSourceLocation attr_location;
};

struct UbsanFunctionTypeMismatchData {
  struct UbsanSourceLocation location;
  struct UbsanTypeDescriptor *type;
};

struct UbsanInvalidValueData {
  struct UbsanSourceLocation location;
  struct UbsanTypeDescriptor *type;
};

struct UbsanOverflowData {
  struct UbsanSourceLocation location;
  struct UbsanTypeDescriptor *type;
};

struct UbsanFloatCastOverflowData {
  struct UbsanSourceLocation location;
  struct UbsanTypeDescriptor *from_type;
  struct UbsanTypeDescriptor *to_type;
};

struct UbsanOutOfBoundsData {
  struct UbsanSourceLocation location;
  struct UbsanTypeDescriptor *arraytype;
  struct UbsanTypeDescriptor *index_type;
};

struct UbsanShiftOutOfBoundsInfo {
  struct UbsanSourceLocation location;
  struct UbsanTypeDescriptor *lhs_type;
  struct UbsanTypeDescriptor *rhs_type;
};

static const char kUbsanTypeCheckKinds[] = "\
load of\0\
store to\0\
reference binding to\0\
member access within\0\
member call on\0\
constructor call on\0\
downcast of\0\
downcast of\0\
upcast of\0\
cast to virtual base of\0\
\0";

static int __ubsan_bits(struct UbsanTypeDescriptor *t) {
  return 1 << (t->info >> 1);
}

static bool __ubsan_signed(struct UbsanTypeDescriptor *t) {
  return t->info & 1;
}

static bool __ubsan_negative(struct UbsanTypeDescriptor *t, uintptr_t x) {
  return __ubsan_signed(t) && (intptr_t)x < 0;
}

static size_t __ubsan_strlen(const char *s) {
  size_t n = 0;
  while (*s++) ++n;
  return n;
}

static char *__ubsan_stpcpy(char *d, const char *s) {
  size_t i;
  for (i = 0;; ++i) {
    if (!(d[i] = s[i])) {
      return d + i;
    }
  }
}

static char *__ubsan_poscpy(char *p, uintptr_t i) {
  int a, b, t, j = 0;
  do {
    p[j++] = i % 10 + '0';
    i /= 10;
  } while (i > 0);
  for (a = 0, b = j - 1; a < b; ++a, --b) {
    t = p[a];
    p[a] = p[b];
    p[b] = t;
  }
  return p + j;
}

static char *__ubsan_intcpy(char *p, intptr_t i) {
  if (i >= 0) return __ubsan_poscpy(p, i);
  *p++ = '-';
  return __ubsan_poscpy(p, -i);
}

static char *__ubsan_hexcpy(char *p, uintptr_t x, int k) {
  while (k) *p++ = "0123456789abcdef"[(x >> (k -= 4)) & 15];
  return p;
}

static char *__ubsan_itpcpy(char *p, struct UbsanTypeDescriptor *t,
                            uintptr_t x) {
  if (__ubsan_signed(t)) {
    return __ubsan_intcpy(p, x);
  } else {
    return __ubsan_poscpy(p, x);
  }
}

static const char *__ubsan_dubnul(const char *s, unsigned i) {
  size_t n;
  while (i--) {
    if ((n = __ubsan_strlen(s))) {
      s += n + 1;
    } else {
      return NULL;
    }
  }
  return s;
}

static uintptr_t __ubsan_extend(struct UbsanTypeDescriptor *t, uintptr_t x) {
  int w;
  w = __ubsan_bits(t);
  if (w < sizeof(x) * CHAR_BIT) {
    x <<= sizeof(x) * CHAR_BIT - w;
    if (__ubsan_signed(t)) {
      x = (intptr_t)x >> w;
    } else {
      x >>= w;
    }
  }
  return x;
}

_Noreturn static void __ubsan_exit(int rc) {
  exit(1);
}

static ssize_t __ubsan_write(const void *data, size_t size) {
  return write(2, data, size);
}

static ssize_t __ubsan_write_string(const char *s) {
  return __ubsan_write(s, __ubsan_strlen(s));
}

_Noreturn void __ubsan_abort(const struct UbsanSourceLocation *loc,
                             const char *description) {
  char buf[1024], *p = buf;
  p = __ubsan_stpcpy(p, "error: ");
  p = __ubsan_stpcpy(p, loc->file), *p++ = ':';
  p = __ubsan_intcpy(p, loc->line);
  p = __ubsan_stpcpy(p, ": ");
  p = __ubsan_stpcpy(p, description);
  p = __ubsan_stpcpy(p, "\r\n");
  __ubsan_write_string(buf);
  __ubsan_exit(134);
}

static const char *__ubsan_describe_shift(
    struct UbsanShiftOutOfBoundsInfo *info, uintptr_t lhs, uintptr_t rhs) {
  if (__ubsan_negative(info->rhs_type, rhs)) {
    return "shift exponent is negative";
  } else if (rhs >= __ubsan_bits(info->lhs_type)) {
    return "shift exponent too large for type";
  } else if (__ubsan_negative(info->lhs_type, lhs)) {
    return "left shift of negative value";
  } else if (__ubsan_signed(info->lhs_type)) {
    return "signed left shift changed sign bit or overflowed";
  } else {
    return "wut shift out of bounds";
  }
}

_Noreturn void __ubsan_handle_shift_out_of_bounds(
    struct UbsanShiftOutOfBoundsInfo *info, uintptr_t lhs, uintptr_t rhs) {
  char buf[512], *p = buf;
  lhs = __ubsan_extend(info->lhs_type, lhs);
  rhs = __ubsan_extend(info->rhs_type, rhs);
  p = __ubsan_stpcpy(p, __ubsan_describe_shift(info, lhs, rhs)), *p++ = ' ';
  p = __ubsan_itpcpy(p, info->lhs_type, lhs), *p++ = ' ';
  p = __ubsan_stpcpy(p, info->lhs_type->name), *p++ = ' ';
  p = __ubsan_itpcpy(p, info->rhs_type, rhs), *p++ = ' ';
  p = __ubsan_stpcpy(p, info->rhs_type->name);
  __ubsan_abort(&info->location, buf);
}

_Noreturn void __ubsan_handle_shift_out_of_bounds_abort(
    struct UbsanShiftOutOfBoundsInfo *info, uintptr_t lhs, uintptr_t rhs) {
  __ubsan_handle_shift_out_of_bounds(info, lhs, rhs);
}

_Noreturn void __ubsan_handle_out_of_bounds(struct UbsanOutOfBoundsInfo *info,
                                            uintptr_t index) {
  char buf[512], *p = buf;
  p = __ubsan_stpcpy(p, info->index_type->name);
  p = __ubsan_stpcpy(p, " index ");
  p = __ubsan_itpcpy(p, info->index_type, index);
  p = __ubsan_stpcpy(p, " into ");
  p = __ubsan_stpcpy(p, info->array_type->name);
  p = __ubsan_stpcpy(p, " out of bounds");
  __ubsan_abort(&info->location, buf);
}

_Noreturn void __ubsan_handle_out_of_bounds_abort(
    struct UbsanOutOfBoundsInfo *info, uintptr_t index) {
  __ubsan_handle_out_of_bounds(info, index);
}

_Noreturn void __ubsan_handle_type_mismatch(struct UbsanTypeMismatchInfo *info,
                                            uintptr_t pointer) {
  const char *kind;
  char buf[512], *p = buf;
  if (!pointer) __ubsan_abort(&info->location, "null pointer access");
  kind = __ubsan_dubnul(kUbsanTypeCheckKinds, info->type_check_kind);
  if (info->alignment && (pointer & (info->alignment - 1))) {
    p = __ubsan_stpcpy(p, "unaligned ");
    p = __ubsan_stpcpy(p, kind), *p++ = ' ';
    p = __ubsan_stpcpy(p, info->type->name), *p++ = ' ', *p++ = '@';
    p = __ubsan_itpcpy(p, info->type, pointer);
    p = __ubsan_stpcpy(p, " align ");
    p = __ubsan_intcpy(p, info->alignment);
    *p = '\0';
  } else {
    p = __ubsan_stpcpy(p, "insufficient size\r\n\t");
    p = __ubsan_stpcpy(p, kind);
    p = __ubsan_stpcpy(p, " address 0x");
    p = __ubsan_hexcpy(p, pointer, sizeof(pointer) * CHAR_BIT);
    p = __ubsan_stpcpy(p, " with insufficient space for object of type ");
    p = __ubsan_stpcpy(p, info->type->name);
  }
  __ubsan_abort(&info->location, buf);
}

_Noreturn void __ubsan_handle_type_mismatch_abort(
    struct UbsanTypeMismatchInfo *info, uintptr_t pointer) {
  __ubsan_handle_type_mismatch(info, pointer);
}

_Noreturn void __ubsan_handle_type_mismatch_v1(
    struct UbsanTypeMismatchInfoClang *type_mismatch, uintptr_t pointer) {
  struct UbsanTypeMismatchInfo mm;
  mm.location = type_mismatch->location;
  mm.type = type_mismatch->type;
  mm.alignment = 1u << type_mismatch->log_alignment;
  mm.type_check_kind = type_mismatch->type_check_kind;
  __ubsan_handle_type_mismatch(&mm, pointer);
}

_Noreturn void __ubsan_handle_type_mismatch_v1_abort(
    struct UbsanTypeMismatchInfoClang *type_mismatch, uintptr_t pointer) {
  __ubsan_handle_type_mismatch_v1(type_mismatch, pointer);
}

_Noreturn void __ubsan_handle_float_cast_overflow(void *data_raw,
                                                  void *from_raw) {
  struct UbsanFloatCastOverflowData *data =
      (struct UbsanFloatCastOverflowData *)data_raw;
  __ubsan_abort(&data->location, "float cast overflow");
}

_Noreturn void __ubsan_handle_float_cast_overflow_abort(void *data_raw,
                                                        void *from_raw) {
  __ubsan_handle_float_cast_overflow(data_raw, from_raw);
}

_Noreturn void __ubsan_handle_add_overflow(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "add overflow");
}

_Noreturn void __ubsan_handle_add_overflow_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_add_overflow(loc);
}

_Noreturn void __ubsan_handle_alignment_assumption(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "alignment assumption");
}

_Noreturn void __ubsan_handle_alignment_assumption_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_alignment_assumption(loc);
}

_Noreturn void __ubsan_handle_builtin_unreachable(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "builtin unreachable");
}

_Noreturn void __ubsan_handle_builtin_unreachable_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_builtin_unreachable(loc);
}

_Noreturn void __ubsan_handle_cfi_bad_type(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "cfi bad type");
}

_Noreturn void __ubsan_handle_cfi_bad_type_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_cfi_bad_type(loc);
}

_Noreturn void __ubsan_handle_cfi_check_fail(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "cfi check fail");
}

_Noreturn void __ubsan_handle_cfi_check_fail_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_cfi_check_fail(loc);
}

_Noreturn void __ubsan_handle_divrem_overflow(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "divrem overflow");
}

_Noreturn void __ubsan_handle_divrem_overflow_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_divrem_overflow(loc);
}

_Noreturn void __ubsan_handle_dynamic_type_cache_miss(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "dynamic type cache miss");
}

_Noreturn void __ubsan_handle_dynamic_type_cache_miss_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_dynamic_type_cache_miss(loc);
}

_Noreturn void __ubsan_handle_function_type_mismatch(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "function type mismatch");
}

_Noreturn void __ubsan_handle_function_type_mismatch_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_function_type_mismatch(loc);
}

_Noreturn void __ubsan_handle_implicit_conversion(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "implicit conversion");
}

_Noreturn void __ubsan_handle_implicit_conversion_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_implicit_conversion(loc);
}

_Noreturn void __ubsan_handle_invalid_builtin(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "invalid builtin");
}

_Noreturn void __ubsan_handle_invalid_builtin_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_invalid_builtin(loc);
}

_Noreturn void __ubsan_handle_load_invalid_value(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "load invalid value (uninitialized? bool∌[01]?)");
}

_Noreturn void __ubsan_handle_load_invalid_value_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_load_invalid_value(loc);
}

_Noreturn void __ubsan_handle_missing_return(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "missing return");
}

_Noreturn void __ubsan_handle_missing_return_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_missing_return(loc);
}

_Noreturn void __ubsan_handle_mul_overflow(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "multiply overflow");
}

_Noreturn void __ubsan_handle_mul_overflow_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_mul_overflow(loc);
}

_Noreturn void __ubsan_handle_negate_overflow(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "negate overflow");
}

_Noreturn void __ubsan_handle_negate_overflow_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_negate_overflow(loc);
}

_Noreturn void __ubsan_handle_nonnull_arg(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "nonnull argument");
}

_Noreturn void __ubsan_handle_nonnull_arg_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_nonnull_arg(loc);
}

_Noreturn void __ubsan_handle_nonnull_return_v1(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "non-null return (v1)");
}

_Noreturn void __ubsan_handle_nonnull_return_v1_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_nonnull_return_v1(loc);
}

_Noreturn void __ubsan_handle_nullability_arg(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "nullability arg");
}

_Noreturn void __ubsan_handle_nullability_arg_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_nullability_arg(loc);
}

_Noreturn void __ubsan_handle_nullability_return_v1(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "nullability return (v1)");
}

_Noreturn void __ubsan_handle_nullability_return_v1_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_nullability_return_v1(loc);
}

_Noreturn void __ubsan_handle_pointer_overflow(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "pointer overflow");
}

_Noreturn void __ubsan_handle_pointer_overflow_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_pointer_overflow(loc);
}

_Noreturn void __ubsan_handle_sub_overflow(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "subtract overflow");
}

_Noreturn void __ubsan_handle_sub_overflow_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_sub_overflow(loc);
}

_Noreturn void __ubsan_handle_vla_bound_not_positive(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "vla bound not positive");
}

_Noreturn void __ubsan_handle_vla_bound_not_positive_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_vla_bound_not_positive(loc);
}

_Noreturn void __ubsan_handle_nonnull_return(
    const struct UbsanSourceLocation *loc) {
  __ubsan_abort(loc, "nonnull return");
}

_Noreturn void __ubsan_handle_nonnull_return_abort(
    const struct UbsanSourceLocation *loc) {
  __ubsan_handle_nonnull_return(loc);
}

void __ubsan_default_options(void) {
}

void __ubsan_on_report(void) {
}

void *__ubsan_get_current_report_data(void) {
  return 0;
}
