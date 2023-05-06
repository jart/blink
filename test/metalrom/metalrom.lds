ENTRY(_start)
OUTPUT_FORMAT(binary)

SECTIONS {
  .text (0xfff0 - ALIGN(_edata - _base, 0x10)) : {
    _base = .;
    *(.text .text.*)
    . = ALIGN(0x10);
    *(.rodata .rodata.*)
    . = ALIGN(0x10);
    *(.data .data.*)
    _edata = .;
  }

  .fixedaddr.0xfff0 0xfff0 : {
    *(.fixedaddr.0xfff0 .fixedaddr.0xfff0.*)
  }

  .fixedaddr.0xffff 0xffff : {
    BYTE(0)
  }

  /DISCARD/ : {
    *(.*)
  }
}
