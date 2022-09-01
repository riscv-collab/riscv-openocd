# this is just a simple assembly file that defines several global symbols
# nothing intresting to see here - however, take a look at example.exp instead
.global _entry
.global main
.global end

.text
_entry:
  nop
main:
  nop
end:
   j main

