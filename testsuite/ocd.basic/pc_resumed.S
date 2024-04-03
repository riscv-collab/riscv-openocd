.global _entry
.global _entry_t1
.global loop
.global sync

.text
_entry:
  la a0, mem
wait:
  amoswap.w.aqrl t0, x0, (a0)
  beqz t0, wait
loop:
  j loop

_entry_t1:
  la a0, mem
  li t0, 1
  amoadd.w.aqrl x0, t0, (a0)
  j loop

.balign 8
mem:
.8byte 0x0
