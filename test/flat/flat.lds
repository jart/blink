ENTRY(_start)
OUTPUT_FORMAT(binary)

SECTIONS {
  . = 0x400000;
  .text : {
    *(.text)
    *(.rodata .rodata.*)
  }
  .bss : {
    *(.bss)
  }
  /DISCARD/ : {
    *(.*)
  }
}
