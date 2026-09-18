OUTPUT_FORMAT("elf64-x86-64", "elf64-x86-64", "elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS { text PT_LOAD FLAGS(7); }

SECTIONS
{
    . = 0;

    .text : {
        *(.text._start)
        *(.text) *(.text.*)
        *(.rodata) *(.rodata.*)
        *(.data) *(.data.*)
        *(.data.rel) *(.data.rel.*)
        *(.data.rel.ro) *(.data.rel.ro.*)
        *(.got) *(.got.plt)

        __bss_start = .;
        *(.bss) *(.bss.*) *(COMMON)
        __bss_end = .;
    } :text

    .rela_out : {
        __rela_start = .;
        *(.rela.dyn) *(.rela.dyn.*)
        *(.rela.text) *(.rela.text.*)
        *(.rela.data) *(.rela.data.*)
        *(.rela.data.rel) *(.rela.data.rel.*)
        *(.rela.rodata) *(.rela.rodata.*)
        *(.rela.bss) *(.rela.bss.*)
        *(.rela.got) *(.rela.got.*)
        *(.rela.init_array) *(.rela.fini_array)
        __rela_end = .;
    } :text

    /DISCARD/ : {
        *(.comment) *(.note.*) *(.eh_frame) *(.eh_frame.*)
        *(.interp) *(.dynsym) *(.dynstr) *(.dynamic)
        *(.gnu.hash) *(.gnu.version*) *(.plt) *(.plt.*)
    }
}
