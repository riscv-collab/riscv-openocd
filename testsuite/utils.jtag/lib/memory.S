.global _entry
.global main
.global end

.text
_entry:
  addi    x1, x0, 1
main:
  addi    x1, x1, 1
end:
   j main

