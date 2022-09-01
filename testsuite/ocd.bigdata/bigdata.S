.global _entry
.global main
.global end
.global work_area_start
.global work_area_end

.text
_entry:
  nop
main:
  nop
end:
  j main

.balign 16
work_area_start:
  .8byte 0xafafafafbbbbeeee
  .fill 1022, 4, 0xdeadbabe
  .fill 1022, 4, 0x80ff00ff
  .8byte 0xcdcdcdcd8899aabb
work_area_end:
  .8byte 0
