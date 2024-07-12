.global _entry
.global main
.global end
.global loop

.text
_entry:
  nop
main:
  la       t0, loop
  csrw     mtvec, t0
end:
  ebreak

.balign 4
loop:
    j loop
  
