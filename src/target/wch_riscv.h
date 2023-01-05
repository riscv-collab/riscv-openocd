#include <target/riscv/riscv.h>

#define get_field(reg, mask) (((reg) & (mask)) / ((mask) & ~((mask) << 1)))
#define set_field(reg, mask, val) (((reg) & ~(mask)) | (((val) * ((mask) & ~((mask) << 1))) & (mask)))

extern const virt2phys_info_t sv32;
extern const virt2phys_info_t sv39;
extern const virt2phys_info_t sv48;
	

extern void riscv_sample_buf_maybe_add_timestamp(struct target *target, bool before);
extern const char *riscv_get_gdb_arch(struct target *target);
extern int parse_ranges(struct list_head *ranges, const char *tcl_arg, const char *reg_type, unsigned int max_val);

extern const char *gdb_regno_name(enum gdb_regno regno);
extern struct target_type wch_riscv013_target;



extern bool riscv_enable_virt2phys ;
extern bool riscv_ebreakm ;
extern bool riscv_ebreaks ;
extern bool riscv_ebreaku ;

extern bool riscv_enable_virtual;
extern int wch_riscv_openocd_poll(struct target *target);