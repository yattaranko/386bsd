OUTPUT_FORMAT("elf32-i386");

SECTIONS
{
    . = 0x98200;
    .text :     {}
    .data :     {}
    .rdata :    {}   /* rdataをここに置いておかないと広大な無駄空間が挿入される */
    .rodata :   {}
    . = ALIGN(4);

	_edata = . ;
	PROVIDE (edata = .) ;   /* edataの定義 */

    .bss :      {}

	_end = . ;
	PROVIDE (end = .) ;     /* endの定義 */

	/DISCARD/ :
	{
		*(.eh_frame)
		*(.note.gnu.property)
        *(.note*)
        *(.comment*)
        *(.drectve*)
        *(.reloc*)
	}
}
