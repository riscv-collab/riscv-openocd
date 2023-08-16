.altmacro

.global _entry
.global end

.macro generate_byte_locations regn
.global address_offset_\regn
address_offset_\regn:
  .byte \regn
.endm

.macro read_write_byte load store regn
.global exec_\load\()_offset_\regn
exec_\load\()_offset_\regn:
  la x1, address_offset_\regn
.global do_\load\()_offset_\regn
do_\load\()_offset_\regn:
  \load x1, 0(x1)
.global exec_\load\()_offset_\regn\()_end
exec_\load\()_offset_\regn\()_end:
  j exec_\load\()_offset_\regn\()_end
.global exec_\store\()_offset_\regn
exec_\store\()_offset_\regn:
  la x1, address_offset_\regn
.global do_\store\()_offset_\regn
do_\store\()_offset_\regn:
  \store x1, 0(x1)
.global exec_\store\()_offset_\regn\()_end
exec_\store\()_offset_\regn\()_end:
  j exec_\store\()_offset_\regn\()_end
.endm

.text
_entry:
  j _entry
.set i,0
.rept 16
   read_write_byte lb, sb, %i
  .set i,i+1
.endr

.set i,0
.rept 8
   read_write_byte lh, sh, %i
  .set i,i+2
.endr

.set i,0
.rept 4
   read_write_byte lw, sw, %i
  .set i,i+4
.endr

#if __riscv_xlen >= 64
.set i,0
.rept 2
   read_write_byte ld, sd, %i
  .set i,i+8
.endr
#endif

end:
  j end

.balign 16
  .fill 4, 4, 0xdeadbabe
.set i,0
.rept 16
  generate_byte_locations %i
  .set i,i+1
.endr
  .fill 4, 4, 0xdeadbabe
