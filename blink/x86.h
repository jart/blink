#ifndef BLINK_X86_H_
#define BLINK_X86_H_
#include <stddef.h>

#include "blink/types.h"

#define XED_MAX_INSTRUCTION_BYTES 15

#define XED_MODE_REAL   0
#define XED_MODE_LEGACY 1
#define XED_MODE_LONG   2

#define xed_sib_base(M)  ((0007 & (M)) >> 0)
#define xed_sib_index(M) ((0070 & (M)) >> 3)
#define xed_sib_scale(M) ((0300 & (M)) >> 6)

#define XED_MACHINE_MODE_REAL           XED_MODE_REAL
#define XED_MACHINE_MODE_LEGACY_32      XED_MODE_LEGACY
#define XED_MACHINE_MODE_LONG_64        XED_MODE_LONG
#define XED_MACHINE_MODE_UNREAL         (1 << 2 | XED_MODE_REAL)
#define XED_MACHINE_MODE_LEGACY_16      (2 << 2 | XED_MODE_REAL)
#define XED_MACHINE_MODE_LONG_COMPAT_16 (3 << 2 | XED_MODE_REAL)
#define XED_MACHINE_MODE_LONG_COMPAT_32 (4 << 2 | XED_MODE_LEGACY)

enum XedError {
  XED_ERROR_NONE,
  XED_ERROR_BUFFER_TOO_SHORT,
  XED_ERROR_GENERAL_ERROR,
  XED_ERROR_INVALID_FOR_CHIP,
  XED_ERROR_BAD_REGISTER,
  XED_ERROR_BAD_LOCK_PREFIX,
  XED_ERROR_BAD_REP_PREFIX,
  XED_ERROR_BAD_LEGACY_PREFIX,
  XED_ERROR_BAD_REX_PREFIX,
  XED_ERROR_BAD_EVEX_UBIT,
  XED_ERROR_BAD_MAP,
  XED_ERROR_BAD_EVEX_V_PRIME,
  XED_ERROR_BAD_EVEX_Z_NO_MASKING,
  XED_ERROR_NO_OUTPUT_POINTER,
  XED_ERROR_NO_AGEN_CALL_BACK_REGISTERED,
  XED_ERROR_BAD_MEMOP_INDEX,
  XED_ERROR_CALLBACK_PROBLEM,
  XED_ERROR_GATHER_REGS,
  XED_ERROR_INSTR_TOO_LONG,
  XED_ERROR_INVALID_MODE,
  XED_ERROR_BAD_EVEX_LL,
  XED_ERROR_UNIMPLEMENTED,
  XED_ERROR_LAST
};

enum XedAddressWidth {
  XED_ADDRESS_WIDTH_INVALID = 0,
  XED_ADDRESS_WIDTH_16b = 2,
  XED_ADDRESS_WIDTH_32b = 4,
  XED_ADDRESS_WIDTH_64b = 8,
  XED_ADDRESS_WIDTH_LAST
};

enum XedIldMap {
  XED_ILD_MAP0, /* 8086+  ... */
  XED_ILD_MAP1, /* 286+   0x0F,... */
  XED_ILD_MAP2, /* Core2+ 0x0F,0x38,... */
  XED_ILD_MAP3, /* Core2+ 0x0F,0x3A,... */
  XED_ILD_MAP4,
  XED_ILD_MAP5,
  XED_ILD_MAP6,
  XED_ILD_MAP_LAST,
  XED_ILD_MAP_INVALID
};

struct XedOperands { /*
  ┌lock
  │ ┌log₂𝑏
  │ │ ┌mode
  │ │ │ ┌eamode
  │ │ │ │ ┌mod
  │ │ │ │ │ ┌asz
  │ │ │ │ │ │┌sego
  │ │ │ │ │ ││  ┌rexx
  │ │ │ │ │ ││  │┌rex         REGISTER
  │ │ │ │ │ ││  ││┌rexb       DISPATCH
  │ │ │ │ │ ││  │││┌srm       ENCODING
  │ │ │ │ │ ││  ││││  ┌rex
  │ │ │ │ │ ││  ││││  │┌rexb
  │ │ │ │ │ ││  ││││  ││┌rm
  │ │ │ │ │ ││  ││││  │││  ┌rexw
  │ │ │ │ │ ││  ││││  │││  │┌osz
  │ │ │ │ │ ││  ││││  │││  ││┌rex
  │ │ │ │ │ ││  ││││  │││  │││┌rexr
  │ │ │ │ │ ││  ││││  │││  ││││┌reg
  │ │2│2│2│2││  ││││  │││  │││││
  │ │8│6│4│2││18││││12│││ 7│││││ 0
  │ ├┐├┐├┐├┐│├─┐│││├─┐││├─┐││││├─┐
  00000000000000000000000000000000*/
  u32 rde;
  u8 sib;
  u8 opcode;
  u8 map; /* enum XedIldMap */
  u8 rep;
  u64 uimm0; /* $immediate mostly sign-extended */
  i64 disp;  /* displacement(%xxx) mostly sign-extended */
  unsigned out_of_bytes : 1;
  unsigned is_intel_specific : 1;
  unsigned has_sib : 1;
  unsigned realmode : 1;
  unsigned has_modrm : 2;
  unsigned imm_signed : 1;    /* internal */
  unsigned disp_unsigned : 1; /* internal */
  u8 error : 5;               /* enum XedError */
  u8 max_bytes;
  u8 uimm1;      /* enter $x,$y */
  u8 disp_width; /* in bits */
  u8 imm_width;  /* in bits */
  u8 pos_opcode;
};

struct XedDecodedInst {
  unsigned char length;
  u8 bytes[15];
  struct XedOperands op;
};

extern const char kXedCopyright[];
extern const char kXedErrorNames[];

struct XedDecodedInst *InitializeInstruction(struct XedDecodedInst *, int);
enum XedError DecodeInstruction(struct XedDecodedInst *, const void *, size_t);

#endif /* BLINK_X86_H_ */
