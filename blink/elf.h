#ifndef BLINK_ELF_H_
#define BLINK_ELF_H_
#include <stddef.h>

#include "blink/types.h"

/**
 * @fileoverview Executable and Linkable Format Definitions.
 */

#define EI_MAG0_   0
#define EI_MAG1_   1
#define EI_MAG2_   2
#define EI_MAG3_   3
#define EI_NIDENT_ 16

#define ELFMAG_  "\177ELF"
#define ELFMAG0_ 0x7f
#define ELFMAG1_ 'E'
#define ELFMAG2_ 'L'
#define ELFMAG3_ 'F'
#define SELFMAG_ 4

#define EI_CLASS_     4
#define ELFCLASSNONE_ 0
#define ELFCLASS32_   1
#define ELFCLASS64_   2
#define ELFCLASSNUM_  3

#define EI_DATA_     5
#define ELFDATANONE_ 0
#define ELFDATA2LSB_ 1
#define ELFDATA2MSB_ 2
#define ELFDATANUM_  3

#define EI_VERSION_ 6

#define EI_OSABI_            7
#define ELFOSABI_NONE_       0
#define ELFOSABI_SYSV_       0
#define ELFOSABI_HPUX_       1
#define ELFOSABI_NETBSD_     2
#define ELFOSABI_LINUX_      3
#define ELFOSABI_GNU_        3
#define ELFOSABI_SOLARIS_    6
#define ELFOSABI_AIX_        7
#define ELFOSABI_IRIX_       8
#define ELFOSABI_FREEBSD_    9
#define ELFOSABI_TRU64_      10
#define ELFOSABI_MODESTO_    11
#define ELFOSABI_OPENBSD_    12
#define ELFOSABI_ARM_        97
#define ELFOSABI_STANDALONE_ 255

#define EI_ABIVERSION_ 8

#define EI_PAD_ 9

#define ET_NONE_   0
#define ET_REL_    1
#define ET_EXEC_   2
#define ET_DYN_    3
#define ET_CORE_   4
#define ET_NUM_    5
#define ET_LOOS_   0xfe00
#define ET_HIOS_   0xfeff
#define ET_LOPROC_ 0xff00
#define ET_HIPROC_ 0xffff

#define EM_NONE_      0
#define EM_M32_       1
#define EM_386_       3
#define EM_S390_      22
#define EM_ARM_       40
#define EM_NEXGEN32E_ 62
#define EM_X86_64_    EM_NEXGEN32E
#define EM_IA32E_     EM_NEXGEN32E
#define EM_AMD64_     EM_NEXGEN32E
#define EM_PDP11_     65
#define EM_CRAYNV2_   172
#define EM_L10M_      180
#define EM_K10M_      181
#define EM_AARCH64_   183
#define EM_CUDA_      190
#define EM_Z80_       220
#define EM_RISCV_     243
#define EM_BPF_       247

#define GRP_COMDAT_ 0x1
#define STN_UNDEF_  0

#define EV_NONE_    0
#define EV_CURRENT_ 1
#define EV_NUM_     2

#define SYMINFO_NONE_          0
#define SYMINFO_CURRENT_       1
#define SYMINFO_NUM_           2
#define SYMINFO_BT_SELF_       0xffff
#define SYMINFO_BT_PARENT_     0xfffe
#define SYMINFO_BT_LOWRESERVE_ 0xff00
#define SYMINFO_FLG_DIRECT_    0x0001
#define SYMINFO_FLG_PASSTHRU_  0x0002
#define SYMINFO_FLG_COPY_      0x0004
#define SYMINFO_FLG_LAZYLOAD_  0x0008

#define PT_NULL_              0
#define PT_LOAD_              1
#define PT_DYNAMIC_           2
#define PT_INTERP_            3
#define PT_NOTE_              4
#define PT_SHLIB_             5
#define PT_PHDR_              6
#define PT_TLS_               7
#define PT_NUM_               8
#define PT_LOOS_              0x60000000
#define PT_GNU_EH_FRAME_      0x6474e550
#define PT_GNU_STACK_         0x6474e551
#define PT_GNU_RELRO_         0x6474e552
#define PT_OPENBSD_RANDOMIZE_ 0x65a3dbe6
#define PT_LOSUNW_            0x6ffffffa
#define PT_SUNWBSS_           0x6ffffffa
#define PT_SUNWSTACK_         0x6ffffffb
#define PT_HISUNW_            0x6fffffff
#define PT_HIOS_              0x6fffffff
#define PT_LOPROC_            0x70000000
#define PT_HIPROC_            0x7fffffff

#define PN_XNUM_ 0xffff

#define PF_X_        1
#define PF_W_        2
#define PF_R_        4
#define PF_MASKOS_   0x0ff00000
#define PF_MASKPROC_ 0xf0000000

#define R_X86_64_NONE_            0
#define R_X86_64_64_              1
#define R_X86_64_PC32_            2
#define R_X86_64_GOT32_           3
#define R_X86_64_PLT32_           4
#define R_X86_64_COPY_            5
#define R_X86_64_GLOB_DAT_        6
#define R_X86_64_JUMP_SLOT_       7
#define R_X86_64_RELATIVE_        8
#define R_X86_64_GOTPCREL_        9
#define R_X86_64_32_              10
#define R_X86_64_32S_             11
#define R_X86_64_16_              12
#define R_X86_64_PC16_            13
#define R_X86_64_8_               14
#define R_X86_64_PC8_             15
#define R_X86_64_DTPMOD64_        16
#define R_X86_64_DTPOFF64_        17
#define R_X86_64_TPOFF64_         18
#define R_X86_64_TLSGD_           19
#define R_X86_64_TLSLD_           20
#define R_X86_64_DTPOFF32_        21
#define R_X86_64_GOTTPOFF_        22
#define R_X86_64_TPOFF32_         23
#define R_X86_64_PC64_            24
#define R_X86_64_GOTOFF64_        25
#define R_X86_64_GOTPC32_         26
#define R_X86_64_GOT64_           27
#define R_X86_64_GOTPCREL64_      28
#define R_X86_64_GOTPC64_         29
#define R_X86_64_GOTPLT64_        30
#define R_X86_64_PLTOFF64_        31
#define R_X86_64_SIZE32_          32
#define R_X86_64_SIZE64_          33
#define R_X86_64_GOTPC32_TLSDESC_ 34
#define R_X86_64_TLSDESC_CALL_    35
#define R_X86_64_TLSDESC_         36
#define R_X86_64_IRELATIVE_       37
#define R_X86_64_RELATIVE64_      38
#define R_X86_64_GOTPCRELX_       41  // 6 bytes
#define R_X86_64_REX_GOTPCRELX_   42  // 7 bytes
#define R_X86_64_NUM_             43

#define STB_LOCAL_      0
#define STB_GLOBAL_     1
#define STB_WEAK_       2
#define STB_NUM_        3
#define STB_LOOS_       10
#define STB_GNU_UNIQUE_ 10
#define STB_HIOS_       12
#define STB_LOPROC_     13
#define STB_HIPROC_     15

#define STT_NOTYPE_    0
#define STT_OBJECT_    1
#define STT_FUNC_      2
#define STT_SECTION_   3
#define STT_FILE_      4
#define STT_COMMON_    5
#define STT_TLS_       6
#define STT_NUM_       7
#define STT_LOOS_      10
#define STT_GNU_IFUNC_ 10
#define STT_HIOS_      12
#define STT_LOPROC_    13
#define STT_HIPROC_    15

#define STV_DEFAULT_   0
#define STV_INTERNAL_  1
#define STV_HIDDEN_    2
#define STV_PROTECTED_ 3

#define SHN_UNDEF_     0
#define SHN_LORESERVE_ 0xff00
#define SHN_LOPROC_    0xff00
#define SHN_BEFORE_    0xff00
#define SHN_AFTER_     0xff01
#define SHN_HIPROC_    0xff1f
#define SHN_LOOS_      0xff20
#define SHN_HIOS_      0xff3f
#define SHN_ABS_       0xfff1
#define SHN_COMMON_    0xfff2
#define SHN_XINDEX_    0xffff
#define SHN_HIRESERVE_ 0xffff

#define SHF_WRITE_            (1 << 0)
#define SHF_ALLOC_            (1 << 1)
#define SHF_EXECINSTR_        (1 << 2)
#define SHF_MERGE_            (1 << 4)
#define SHF_STRINGS_          (1 << 5)
#define SHF_INFO_LINK_        (1 << 6)
#define SHF_LINK_ORDER_       (1 << 7)
#define SHF_OS_NONCONFORMING_ (1 << 8)
#define SHF_GROUP_            (1 << 9)
#define SHF_TLS_              (1 << 10)
#define SHF_COMPRESSED_       (1 << 11)
#define SHF_MASKOS_           0x0ff00000
#define SHF_MASKPROC_         0xf0000000
#define SHF_ORDERED_          (1 << 30)
#define SHF_EXCLUDE_          (1U << 31)

#define ELFCOMPRESS_ZLIB_   1
#define ELFCOMPRESS_LOOS_   0x60000000
#define ELFCOMPRESS_HIOS_   0x6fffffff
#define ELFCOMPRESS_LOPROC_ 0x70000000
#define ELFCOMPRESS_HIPROC_ 0x7fffffff

#define SHT_NULL_           0
#define SHT_PROGBITS_       1
#define SHT_SYMTAB_         2
#define SHT_STRTAB_         3
#define SHT_RELA_           4
#define SHT_HASH_           5
#define SHT_DYNAMIC_        6
#define SHT_NOTE_           7
#define SHT_NOBITS_         8
#define SHT_REL_            9
#define SHT_SHLIB_          10
#define SHT_DYNSYM_         11
#define SHT_INIT_ARRAY_     14
#define SHT_FINI_ARRAY_     15
#define SHT_PREINIT_ARRAY_  16
#define SHT_GROUP_          17
#define SHT_SYMTAB_SHNDX_   18
#define SHT_NUM_            19
#define SHT_LOOS_           0x60000000
#define SHT_GNU_ATTRIBUTES_ 0x6ffffff5
#define SHT_GNU_HASH_       0x6ffffff6
#define SHT_GNU_LIBLIST_    0x6ffffff7
#define SHT_CHECKSUM_       0x6ffffff8
#define SHT_LOSUNW_         0x6ffffffa
#define SHT_SUNW_move_      0x6ffffffa
#define SHT_SUNW_COMDAT_    0x6ffffffb
#define SHT_SUNW_syminfo_   0x6ffffffc
#define SHT_GNU_verdef_     0x6ffffffd
#define SHT_GNU_verneed_    0x6ffffffe
#define SHT_GNU_versym_     0x6fffffff
#define SHT_HISUNW_         0x6fffffff
#define SHT_HIOS_           0x6fffffff
#define SHT_LOPROC_         0x70000000
#define SHT_HIPROC_         0x7fffffff
#define SHT_LOUSER_         0x80000000
#define SHT_HIUSER_         0x8fffffff

#define DT_NULL_               0
#define DT_NEEDED_             1
#define DT_PLTRELSZ_           2
#define DT_PLTGOT_             3
#define DT_HASH_               4
#define DT_STRTAB_             5
#define DT_SYMTAB_             6
#define DT_RELA_               7
#define DT_RELASZ_             8
#define DT_RELAENT_            9
#define DT_STRSZ_              10
#define DT_SYMENT_             11
#define DT_INIT_               12
#define DT_FINI_               13
#define DT_SONAME_             14
#define DT_RPATH_              15
#define DT_SYMBOLIC_           16
#define DT_REL_                17
#define DT_RELSZ_              18
#define DT_RELENT_             19
#define DT_PLTREL_             20
#define DT_DEBUG_              21
#define DT_TEXTREL_            22
#define DT_JMPREL_             23
#define DT_BIND_NOW_           24
#define DT_INIT_ARRAY_         25
#define DT_FINI_ARRAY_         26
#define DT_INIT_ARRAYSZ_       27
#define DT_FINI_ARRAYSZ_       28
#define DT_RUNPATH_            29
#define DT_FLAGS_              30
#define DT_ENCODING_           32
#define DT_PREINIT_ARRAY_      32
#define DT_PREINIT_ARRAYSZ_    33
#define DT_SYMTAB_SHNDX_       34
#define DT_NUM_                35
#define DT_LOOS_               0x6000000d
#define DT_HIOS_               0x6ffff000
#define DT_LOPROC_             0x70000000
#define DT_HIPROC_             0x7fffffff
#define DT_VALRNGLO_           0x6ffffd00
#define DT_GNU_PRELINKED_      0x6ffffdf5
#define DT_GNU_CONFLICTSZ_     0x6ffffdf6
#define DT_GNU_LIBLISTSZ_      0x6ffffdf7
#define DT_CHECKSUM_           0x6ffffdf8
#define DT_PLTPADSZ_           0x6ffffdf9
#define DT_MOVEENT_            0x6ffffdfa
#define DT_MOVESZ_             0x6ffffdfb
#define DT_FEATURE_1_          0x6ffffdfc
#define DT_POSFLAG_1_          0x6ffffdfd
#define DT_SYMINSZ_            0x6ffffdfe
#define DT_SYMINENT_           0x6ffffdff
#define DT_VALRNGHI_           0x6ffffdff
#define DT_VALTAGIDX_(tag)     (DT_VALRNGHI - (tag))
#define DT_VALNUM_             12
#define DT_ADDRRNGLO_          0x6ffffe00
#define DT_GNU_HASH_           0x6ffffef5
#define DT_TLSDESC_PLT_        0x6ffffef6
#define DT_TLSDESC_GOT_        0x6ffffef7
#define DT_GNU_CONFLICT_       0x6ffffef8
#define DT_GNU_LIBLIST_        0x6ffffef9
#define DT_CONFIG_             0x6ffffefa
#define DT_DEPAUDIT_           0x6ffffefb
#define DT_AUDIT_              0x6ffffefc
#define DT_PLTPAD_             0x6ffffefd
#define DT_MOVETAB_            0x6ffffefe
#define DT_SYMINFO_            0x6ffffeff
#define DT_ADDRRNGHI_          0x6ffffeff
#define DT_ADDRTAGIDX_(tag)    (DT_ADDRRNGHI - (tag))
#define DT_ADDRNUM_            11
#define DT_VERSYM_             0x6ffffff0
#define DT_RELACOUNT_          0x6ffffff9
#define DT_RELCOUNT_           0x6ffffffa
#define DT_FLAGS_1_            0x6ffffffb
#define DT_VERDEF_             0x6ffffffc
#define DT_VERDEFNUM_          0x6ffffffd
#define DT_VERNEED_            0x6ffffffe
#define DT_VERNEEDNUM_         0x6fffffff
#define DT_VERSIONTAGIDX_(tag) (DT_VERNEEDNUM - (tag))
#define DT_VERSIONTAGNUM_      16
#define DT_AUXILIARY_          0x7ffffffd
#define DT_FILTER_             0x7fffffff
#define DT_EXTRATAGIDX_(tag)   ((Elf32_Word) - ((Elf32_Sword)(tag) << 1 >> 1) - 1)
#define DT_EXTRANUM_           3

#define VER_NEED_NONE_    0
#define VER_NEED_CURRENT_ 1
#define VER_NEED_NUM_     2
#define VER_FLG_WEAK_     0x2

#define ELF_NOTE_SOLARIS_       "SUNW Solaris"
#define ELF_NOTE_GNU_           "GNU"
#define ELF_NOTE_PAGESIZE_HINT_ 1
#define ELF_NOTE_ABI_           NT_GNU_ABI_TAG
#define ELF_NOTE_OS_LINUX_      0
#define ELF_NOTE_OS_GNU_        1
#define ELF_NOTE_OS_SOLARIS2_   2
#define ELF_NOTE_OS_FREEBSD_    3

#define NT_GNU_ABI_TAG_      1
#define NT_GNU_BUILD_ID_     3
#define NT_GNU_GOLD_VERSION_ 4

#define EF_CPU32_ 0x00810000

#define DF_ORIGIN_       0x00000001
#define DF_SYMBOLIC_     0x00000002
#define DF_TEXTREL_      0x00000004
#define DF_BIND_NOW_     0x00000008
#define DF_STATIC_TLS_   0x00000010
#define DF_1_NOW_        0x00000001
#define DF_1_GLOBAL_     0x00000002
#define DF_1_GROUP_      0x00000004
#define DF_1_NODELETE_   0x00000008
#define DF_1_LOADFLTR_   0x00000010
#define DF_1_INITFIRST_  0x00000020
#define DF_1_NOOPEN_     0x00000040
#define DF_1_ORIGIN_     0x00000080
#define DF_1_DIRECT_     0x00000100
#define DF_1_TRANS_      0x00000200
#define DF_1_INTERPOSE_  0x00000400
#define DF_1_NODEFLIB_   0x00000800
#define DF_1_NODUMP_     0x00001000
#define DF_1_CONFALT_    0x00002000
#define DF_1_ENDFILTEE_  0x00004000
#define DF_1_DISPRELDNE_ 0x00008000
#define DF_1_DISPRELPND_ 0x00010000
#define DF_1_NODIRECT_   0x00020000
#define DF_1_IGNMULDEF_  0x00040000
#define DF_1_NOKSYMS_    0x00080000
#define DF_1_NOHDR_      0x00100000
#define DF_1_EDITED_     0x00200000
#define DF_1_NORELOC_    0x00400000
#define DF_1_SYMINTPOSE_ 0x00800000
#define DF_1_GLOBAUDIT_  0x01000000
#define DF_1_SINGLETON_  0x02000000
#define DF_1_STUB_       0x04000000
#define DF_1_PIE_        0x08000000
#define DTF_1_PARINIT_   0x00000001
#define DTF_1_CONFEXP_   0x00000002
#define DF_P1_LAZYLOAD_  0x00000001
#define DF_P1_GROUPPERM_ 0x00000002

#define ELF64_ST_BIND_(val)        (((u8)(val)) >> 4)
#define ELF64_ST_TYPE_(val)        ((val)&0xf)
#define ELF64_ST_INFO_(bind, type) (((bind) << 4) + ((type)&0xf))
#define ELF64_ST_VISIBILITY_(o)    ((o)&0x03)

#define ELF64_R_SYM_(i)          ((i) >> 32)
#define ELF64_R_TYPE_(i)         ((i)&0xffffffff)
#define ELF64_R_INFO_(sym, type) ((((u64)(sym)) << 32) + (type))

#define ELF64_M_SYM_(info)       ((info) >> 8)
#define ELF64_M_SIZE_(info)      ((u8)(info))
#define ELF64_M_INFO_(sym, size) (((sym) << 8) + (u8)(size))

#define NT_PRSTATUS_         1
#define NT_PRFPREG_          2
#define NT_FPREGSET_         2
#define NT_PRPSINFO_         3
#define NT_PRXREG_           4
#define NT_TASKSTRUCT_       4
#define NT_PLATFORM_         5
#define NT_AUXV_             6
#define NT_GWINDOWS_         7
#define NT_ASRS_             8
#define NT_PSTATUS_          10
#define NT_PSINFO_           13
#define NT_PRCRED_           14
#define NT_UTSNAME_          15
#define NT_LWPSTATUS_        16
#define NT_LWPSINFO_         17
#define NT_PRFPXREG_         20
#define NT_SIGINFO_          0x53494749
#define NT_FILE_             0x46494c45
#define NT_PRXFPREG_         0x46e62b7f
#define NT_PPC_VMX_          0x100
#define NT_PPC_SPE_          0x101
#define NT_PPC_VSX_          0x102
#define NT_PPC_TAR_          0x103
#define NT_PPC_PPR_          0x104
#define NT_PPC_DSCR_         0x105
#define NT_PPC_EBB_          0x106
#define NT_PPC_PMU_          0x107
#define NT_PPC_TM_CGPR_      0x108
#define NT_PPC_TM_CFPR_      0x109
#define NT_PPC_TM_CVMX_      0x10a
#define NT_PPC_TM_CVSX_      0x10b
#define NT_PPC_TM_SPR_       0x10c
#define NT_PPC_TM_CTAR_      0x10d
#define NT_PPC_TM_CPPR_      0x10e
#define NT_PPC_TM_CDSCR_     0x10f
#define NT_X86_XSTATE_       0x202
#define NT_S390_HIGH_GPRS_   0x300
#define NT_S390_TIMER_       0x301
#define NT_S390_TODCMP_      0x302
#define NT_S390_TODPREG_     0x303
#define NT_S390_CTRS_        0x304
#define NT_S390_PREFIX_      0x305
#define NT_S390_LAST_BREAK_  0x306
#define NT_S390_SYSTEM_CALL_ 0x307
#define NT_S390_TDB_         0x308
#define NT_S390_VXRS_LOW_    0x309
#define NT_S390_VXRS_HIGH_   0x30a
#define NT_S390_GS_CB_       0x30b
#define NT_S390_GS_BC_       0x30c
#define NT_S390_RI_CB_       0x30d
#define NT_ARM_VFP_          0x400
#define NT_ARM_TLS_          0x401
#define NT_ARM_HW_BREAK_     0x402
#define NT_ARM_HW_WATCH_     0x403
#define NT_ARM_SYSTEM_CALL_  0x404
#define NT_ARM_SVE_          0x405
#define NT_ARM_PAC_MASK_     0x406
#define NT_METAG_CBUF_       0x500
#define NT_METAG_RPIPE_      0x501
#define NT_METAG_TLS_        0x502
#define NT_ARC_V2_           0x600
#define NT_VMCOREDD_         0x700
#define NT_VERSION_          1

#define VER_DEF_NONE_      0
#define VER_DEF_CURRENT_   1
#define VER_DEF_NUM_       2
#define VER_FLG_BASE_      0x1
#define VER_FLG_WEAK_      0x2
#define VER_NDX_LOCAL_     0
#define VER_NDX_GLOBAL_    1
#define VER_NDX_LORESERVE_ 0xff00
#define VER_NDX_ELIMINATE_ 0xff01

#define LL_NONE_           0
#define LL_EXACT_MATCH_    (1 << 0)
#define LL_IGNORE_INT_VER_ (1 << 1)
#define LL_REQUIRE_MINOR_  (1 << 2)
#define LL_EXPORTS_        (1 << 3)
#define LL_DELAY_LOAD_     (1 << 4)
#define LL_DELTA_          (1 << 5)

#define R_BPF_NONE_   0
#define R_BPF_MAP_FD_ 1

typedef struct Elf64_Ehdr_ {
  u8 ident[EI_NIDENT_];
  u8 type[2];       // u16
  u8 machine[2];    // u16
  u8 version[4];    // u32
  u8 entry[8];      // u64
  u8 phoff[8];      // u64
  u8 shoff[8];      // u64
  u8 flags[4];      // u32
  u8 ehsize[2];     // u16
  u8 phentsize[2];  // u16
  u8 phnum[2];      // u16
  u8 shentsize[2];  // u16
  u8 shnum[2];      // u16
  u8 shstrndx[2];   // u16
} Elf64_Ehdr_;

typedef struct Elf64_Phdr_ {
  u8 type[4];    // u32
  u8 flags[4];   // u32
  u8 offset[8];  // u64
  u8 vaddr[8];   // u64
  u8 paddr[8];   // u64
  u8 filesz[8];  // u64
  u8 memsz[8];   // u64
  u8 align[8];   // u64
} Elf64_Phdr_;

typedef struct Elf64_Shdr_ {
  u8 name[4];       // u32
  u8 type[4];       // u32
  u8 flags[8];      // u64
  u8 addr[8];       // u64
  u8 offset[8];     // u64
  u8 size[8];       // u64
  u8 link[4];       // u32
  u8 info[4];       // u32
  u8 addralign[8];  // u64
  u8 entsize[8];    // u64
} Elf64_Shdr_;

typedef struct Elf64_Rel_ {
  u8 offset[8];  // u64
  u8 info[8];    // u64
} Elf64_Rel_;

typedef struct Elf64_Rela_ {
  u8 offset[8];  // u64
  u8 info[8];    // u64
  u8 addend[8];  // i64
} Elf64_Rela_;

typedef struct Elf64_Sym_ {
  u8 name[4];  // u32
  u8 info;
  u8 other;
  u8 shndx[2];  // u16
  u8 value[8];  // u64
  u8 size[8];   // u64
} Elf64_Sym_;

typedef struct Elf64_Syminfo_ {
  u8 boundto[2];  // u16
  u8 flags[2];    // u16
} Elf64_Syminfo_;

typedef struct Elf64_Chdr_ {
  u8 type[4];       // u32
  u8 reserved[4];   // u32
  u8 size[8];       // u64
  u8 addralign[8];  // u64
} Elf64_Chdr_;

typedef struct Elf64_Dyn_ {
  u8 tag[8];  // i64
  u8 val[8];  // u64
} Elf64_Dyn_;

typedef struct Elf64_Lib_ {
  u8 name[4];        // u32
  u8 time_stamp[4];  // u32
  u8 checksum[4];    // u32
  u8 version[4];     // u32
  u8 flags[4];       // u32
} Elf64_Lib_;

typedef struct Elf64_Move_ {
  u8 value[8];    // u64
  u8 info[8];     // u64
  u8 poffset[8];  // u64
  u8 repeat[2];   // u16
  u8 stride[2];   // u16
} Elf64_Move_;

typedef struct Elf64_Nhdr_ {
  u8 namesz[4];  // u32
  u8 descsz[4];  // u32
  u8 type[4];    // u32
} Elf64_Nhdr_;

Elf64_Phdr_ *GetElfProgramHeaderAddress(const Elf64_Ehdr_ *, size_t, u16);
char *GetElfStringTable(const Elf64_Ehdr_ *, size_t);
Elf64_Shdr_ *GetElfSectionHeaderAddress(const Elf64_Ehdr_ *, size_t, u16);
const char *GetElfSectionName(const Elf64_Ehdr_ *, size_t, Elf64_Shdr_ *);
char *GetElfString(const Elf64_Ehdr_ *, size_t, const char *, size_t);
char *GetElfSectionNameStringTable(const Elf64_Ehdr_ *, size_t);
void *GetElfSectionAddress(const Elf64_Ehdr_ *, size_t, const Elf64_Shdr_ *);
Elf64_Sym_ *GetElfSymbolTable(const Elf64_Ehdr_ *, size_t, int *);
i64 GetElfMemorySize(const Elf64_Ehdr_ *, size_t, i64 *);

#endif /* BLINK_ELF_H_ */
