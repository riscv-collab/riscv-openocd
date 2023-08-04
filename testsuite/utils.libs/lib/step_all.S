.global _entry
.global load_address

# option is important since test relies on the fact that instructions are 4 byte
.option norvc
.text
_entry:
addi x1, zero, 1
add x1, x1, x1
add x1, x1, x1
add x1, x1, x1
add x1, x1, x1
j _entry
