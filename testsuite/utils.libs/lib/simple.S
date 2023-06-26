.global _entry
.global load_address

# note: this file is tightly coupled with BINARY_FILE.bin in the same directory
.option norvc
.text
load_address:
j load_address
_entry:
j _entry
