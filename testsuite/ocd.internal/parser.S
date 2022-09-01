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
  .fill 2048, 4, 0xdeadbabe
work_area_end:
   nop
