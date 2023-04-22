ENTRY(_start)
OUTPUT_FORMAT(binary)

SECTIONS {
  . = 0x7c00;

  .head : {
    _base = .;
    *(.head)

    /*
     * Fabricate a degenerate "hard disk partition table" that covers at
     * least this bare metal program.  Fill up 1 partition table entry.
     */
    . =	0x1be;
    BYTE(0x80)				/* 0=non-boot / 0x80=active */
    BYTE(0)				/* start head */
    BYTE(1)				/* start sector[5:0], cylinder[9:8] */
    BYTE(0)				/* start cylinder[7:0] */
    BYTE(0x7f)				/* filesystem type */
    BYTE(0xff)				/* end head */
    BYTE(0xff)				/* end sector[5:0], cylinder[9:8] */
    BYTE(0xff)				/* end cylinder[7:0] */
    LONG(0)				/* c₀*Cₙ + h₀*Hₙ + s₀*Sₙ */
    LONG(0xffffffff)			/* sector count */

    /* Add the boot record signature. */
    . = 0x1fe;
    SHORT(0xaa55);
  } = 0

  .text : {
    *(.text .text.*)
    . = ALIGN(0x10);
    *(.rodata .rodata.*)
    . = ALIGN(0x10);
    *(.data .data.*)
    _edata = .;
  }

  .bss : {
    *(.bss .bss.*)
    *(COMMON)
  }

  /DISCARD/ : {
    *(.*)
  }

  _sectors = ALIGN(_edata - _base, 512) / 512;
}
