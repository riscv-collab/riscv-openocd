.global _entry
.global main
.global end

.text
.balign 4096
_entry:
  addi    x1, x0, 1
main:
  addi    x1, x1, 1
end:
   j main

