.global _entry
.global _entry_t1
.global loop1
.global loop2
.global sync_mem

.text
_entry:
  nop
  la a0, sync_mem
wait:
  amoadd.w.aqrl t0, zero, (a0)
  beqz t0, wait
loop1:
  j loop1

_entry_t1:
  la a0, sync_mem
  li t0, 1
  amoadd.w.aqrl x0, t0, (a0)
loop2:
  j loop2

.balign 64
sync_mem:
.8byte 0x0
