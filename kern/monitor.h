#ifndef JOS_KERN_MONITOR_H
#define JOS_KERN_MONITOR_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

struct Trapframe;

// Activate the kernel monitor,
// optionally providing a trap frame indicating the current state
// (NULL if none).
void monitor(struct Trapframe *tf);

// Functions implementing monitor commands.
int mon_help(int argc, char **argv, struct Trapframe *tf);
int mon_kerninfo(int argc, char **argv, struct Trapframe *tf);
int mon_backtrace(int argc, char **argv, struct Trapframe *tf);
int mon_showmappings(int argc, char **argv, struct Trapframe *tf);
int mon_chperm(int argc, char **argv, struct Trapframe *tf);
int mon_exit(int argc, char **argv, struct Trapframe *tf);
int mon_dump(int argc, char **argv, struct Trapframe *tf);
int mon_dumpp(int argc, char **argv, struct Trapframe *tf);
int mon_showvas(int argc, char **argv, struct Trapframe *tf);

// Functions which interact with the kernel in ways which are useful for
// multiple monitor commands
int get_virtual_addresses_for_pa(physaddr_t addr, uintptr_t* found_vas, uint32_t found_vas_len);

#endif	// !JOS_KERN_MONITOR_H
