ENTRY(ehdr)

SECTIONS {
  . = 0x400000;
  .text : {
    *(.text)
    *(.rodata .rodata.*)
  }
  filesz = . - ehdr;
  textsz = . - ehdr;
  .bss ALIGN(4096) : {
    bss = .;
    *(.bss)
    . = ALIGN(4096);
  }
  memsz = . - ehdr;
  /DISCARD/ : {
    *(.*)
  }
}

bsssize = SIZEOF(.bss);
textoff = ehdr - ehdr;
