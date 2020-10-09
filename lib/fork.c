// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800

//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	if (!(err & FEC_WR) || !(uvpt[(uintptr_t) addr/PGSIZE] & PTE_COW)){
		panic("page fault on new fork for va %x", addr);
	}

	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.

	sys_page_alloc(0,PFTEMP,PTE_U|PTE_P|PTE_W);
	memcpy(PFTEMP,ROUNDDOWN(addr,PGSIZE),PGSIZE);
	sys_page_map(0,PFTEMP,0,ROUNDDOWN(addr,PGSIZE),PTE_U|PTE_P|PTE_W);
	sys_page_unmap(0,PFTEMP);
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r, perm;
	perm = PTE_U | PTE_P;
	if (uvpt[pn] & (PTE_W | PTE_COW)){
		perm |= PTE_COW;
	}
	r = sys_page_map(0, (void *) (pn*PGSIZE), envid, (void *) (pn*PGSIZE), perm);
	r |= sys_page_map(0, (void *) (pn*PGSIZE), 0, (void *) (pn*PGSIZE), perm);
	
	return r;
}


extern void _pgfault_upcall(void);

// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	int r = 0;
	set_pgfault_handler(pgfault);
	envid_t child = sys_exofork();
	if (child < 0)
		return child;
	if (child){
		for (int i = 0; i < NPTENTRIES*NPDENTRIES; i++){
			if (!(uvpd[i/NPTENTRIES] & PTE_P))
				continue;
			if (i*PGSIZE == UXSTACKTOP-PGSIZE){
				r |= sys_page_alloc(child,(void *) (i*PGSIZE), PTE_U|PTE_P|PTE_W);
			}
			else if (uvpt[i] & PTE_P){
				r |= duppage(child, i);
			}
		}
		sys_env_set_pgfault_upcall(child, _pgfault_upcall);
		sys_env_set_status(child,ENV_RUNNABLE);
	}
	else {
		thisenv = &envs[ENVX(sys_getenvid())];
	}
	return r ? r : child;
}

// Challenge!
int
sfork(void)
{
	int r = 0;
	set_pgfault_handler(pgfault);
	envid_t child = sys_exofork();
	if (child < 0)
		return child;
	if (child){
		for (int i = 0; i < NPTENTRIES*NPDENTRIES; i++){
			if (!(uvpd[i/NPTENTRIES] & PTE_P))
				continue;
			if (i*PGSIZE == UXSTACKTOP-PGSIZE){
				r |= sys_page_alloc(child,(void *) (i*PGSIZE), PTE_U|PTE_P|PTE_W);
			}
			else if (i*PGSIZE == USTACKTOP-PGSIZE){
				r |= duppage(child, i);
			}
			else if (uvpt[i] & PTE_P){
				r |= sys_page_map(0, (void *) (i*PGSIZE),
						  child, (void *) (i*PGSIZE), 
						  uvpt[i] & PTE_SYSCALL);
			}
		}
		sys_env_set_pgfault_upcall(child, _pgfault_upcall);
		sys_env_set_status(child,ENV_RUNNABLE);
	}
	else {
		thisenv = &envs[ENVX(sys_getenvid())];
	}
	return r ? r : child;
}
