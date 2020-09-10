// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/pmap.h>
#include <kern/kdebug.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line


struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
	{ "backtrace", "Display backtrace", mon_backtrace },
        { "showmappings", "Display physical pages mapped to virtual addresses", mon_showmappings },
        { "chperm", "Change the permissions for a page of virtual memory", mon_chperm },
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	uint32_t *base = (uint32_t*) read_ebp();
	uintptr_t ip;
	struct Eipdebuginfo info;
	while (base) {
		ip = base[1];
		cprintf("ebp %08x  eip %08x  args %08x %08x %08x %08x %08x\n",
				(uint32_t) base, ip, base[2], base[3], base[4], base[5], base[6]);
		if (debuginfo_eip(ip, &info) != -1) {
			cprintf("\t %s:%d: %.*s+%d\n", info.eip_file, info.eip_line, info.eip_fn_namelen, info.eip_fn_name, (ip - info.eip_fn_addr));
		}

		base = (uint32_t*) *base;
	}
	return 0;
}

int
mon_showmappings(int argc, char **argv, struct Trapframe *tf)
{
	uintptr_t start_va, end_va;
	if (argc == 2) {
		// assumes the argument is specified in hex.
		start_va = end_va = strtol(argv[1]+2, NULL, 16);
	}
	else if (argc == 3) {
		// assumes the argument is specified in hex.
		start_va = strtol(argv[1]+2, NULL, 16);
		end_va = strtol(argv[2]+2, NULL, 16);
	}
	else {
		// complain.
		cprintf("Usage: showmappings start_va [end_va]\nHere start_va and end_va are specified in hex.\n");
		return 1;
	}

	// actually do the thing
	// We want to loop over all possible virtual pages
	// 	For each entry, we want to look up the corresponding entry.
	// 	(maybe also get the permissions from the directory entry)
	// First set up the bounds
	
	for (start_va = ROUNDDOWN(start_va, PGSIZE); start_va <= end_va; start_va += PGSIZE) {
		pte_t* entry = pgdir_walk(kern_pgdir, (void *) start_va, 0);
		
		cprintf("0x%08x ", start_va);
		if (entry && (*entry & PTE_P)) {
			// entry is present
			cprintf("-> 0x%08x ", PTE_ADDR(*entry));
			// compute flags
			pde_t dir_entry = kern_pgdir[PDX(start_va)];

			// print out write and user permissions, and accessed and dirty bits
			cprintf("(%s%s%s%s)\n", 
				(dir_entry & *entry & PTE_W) ? "W":"R",
				(dir_entry & *entry & PTE_U) ? "U":"K",
				(*entry & PTE_A) ? "A":"",
				(*entry & PTE_D) ? "D":""
				);
		} else {
			// entry is not present
			cprintf("not present\n");
		}
	}

	return 0;
}

int
mon_chperm(int argc, char **argv, struct Trapframe *tf)
{
	if (argc != 3) {
		cprintf("Usage: chperm va perm\n");
		cprintf("va is the virtual address in hex.\n");
		cprintf("Valid permissions are 0 (RK), 1 (WK), 2 (RU), 3 (WU).\n");
		return 1;
	}
	
	// assumes the argument is specified in hex.
	uintptr_t va = strtol(argv[1]+2, NULL, 16);
	uintptr_t permissions = strtol(argv[2], NULL, 10);

	if (permissions < 0 || permissions > 3) {
		cprintf("permissions not valid.\n");
		cprintf("Valid permissions are 0 (RK), 1 (WK), 2 (RU), 3 (WU).\n");
		return 2;
	}

	pte_t *entry = pgdir_walk(kern_pgdir, (void *) va, 0);
	if (!entry) {
		cprintf("page not present\n");
		return 3;
	}

	*entry = (*entry & (~0x6 | (permissions << 1))) | (permissions << 1);

	return 0;
}


/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
