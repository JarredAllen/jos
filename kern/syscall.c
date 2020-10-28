/* See COPYRIGHT for copyright information. */

#include <inc/x86.h>
#include <inc/error.h>
#include <inc/string.h>
#include <inc/assert.h>

#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/trap.h>
#include <kern/spinlock.h>
#include <kern/syscall.h>
#include <kern/console.h>
#include <kern/sched.h>

// Print a string to the system console.
// The string is exactly 'len' characters long.
// Destroys the environment on memory errors.
static void
sys_cputs(const char *s, size_t len)
{
	// Check that the user has permission to read memory [s, s+len).
	// Destroy the environment if not.

	user_mem_assert(curenv, s, len, PTE_U);

	// Print the string supplied by the user.
	cprintf("%.*s", len, s);
}

// Read a character from the system console without blocking.
// Returns the character, or 0 if there is no input waiting.
static int
sys_cgetc(void)
{
	return cons_getc();
}

// Returns the current environment's envid.
static envid_t
sys_getenvid(void)
{
	return curenv->env_id;
}

// Destroy a given environment (possibly the currently running environment).
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_destroy(envid_t envid)
{
	int r;
	struct Env *e;

	if ((r = envid2env(envid, &e, 1)) < 0)
		return r;
	if (e == curenv)
		cprintf("[%08x] exiting gracefully\n", curenv->env_id);
	else
		cprintf("[%08x] destroying %08x\n", curenv->env_id, e->env_id);
	env_destroy(e);
	return 0;
}

// Deschedule current environment and pick a different one to run.
static void
sys_yield(void)
{
	sched_yield();
}

// Allocate a new environment.
// Returns envid of new environment, or < 0 on error.  Errors are:
//	-E_NO_FREE_ENV if no free environment is available.
//	-E_NO_MEM on memory exhaustion.
static envid_t
sys_exofork(void)
{
	// Create the new environment with env_alloc(), from kern/env.c.
	// It should be left as env_alloc created it, except that
	// status is set to ENV_NOT_RUNNABLE, and the register set is copied
	// from the current environment -- but tweaked so sys_exofork
	// will appear to return 0.

	struct Env *newproc;
	int ret_code = env_alloc(&newproc, curenv->env_id);
	if (ret_code == -E_NO_FREE_ENV || ret_code == -E_NO_MEM) {
		return ret_code;
	}
	newproc->env_status = ENV_NOT_RUNNABLE;
	newproc->env_tf = curenv->env_tf;
	newproc->env_tf.tf_regs.reg_eax = 0;
	return newproc->env_id;
}

// Set envid's env_status to status, which must be ENV_RUNNABLE
// or ENV_NOT_RUNNABLE.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if status is not a valid status for an environment.
static int
sys_env_set_status(envid_t envid, int status)
{
	struct Env * env;
	int retcode = envid2env(envid, &env, 1);
	if (retcode) {
		return -E_BAD_ENV;
	}
	switch (status) {
		case ENV_NOT_RUNNABLE:
		case ENV_RUNNABLE:
			env->env_status = status;
			return 0;
		default:
			return -E_INVAL;
	}
}

// Set the page fault upcall for 'envid' by modifying the corresponding struct
// Env's 'env_pgfault_upcall' field.  When 'envid' causes a page fault, the
// kernel will push a fault record onto the exception stack, then branch to
// 'func'.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
static int
sys_env_set_pgfault_upcall(envid_t envid, void *func)
{

	struct Env * env;
	int retcode = envid2env(envid, &env, 1);
	if (retcode) {
		return -E_BAD_ENV;
	}
	env->env_pgfault_upcall = func;
	return 0;
}

// Allocate a page of memory and map it at 'va' with permission
// 'perm' in the address space of 'envid'.
// The page's contents are set to 0.
// If a page is already mapped at 'va', that page is unmapped as a
// side effect.
//
// perm -- PTE_U | PTE_P must be set, PTE_AVAIL | PTE_W may or may not be set,
//         but no other bits may be set.  See PTE_SYSCALL in inc/mmu.h.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
//	-E_INVAL if perm is inappropriate (see above).
//	-E_NO_MEM if there's no memory to allocate the new page,
//		or to allocate any necessary page tables.
static int
sys_page_alloc(envid_t envid, void *va, int perm)
{
	struct Env * env;
	int retcode = envid2env(envid, &env, 1);
	if (retcode) {
		return -E_BAD_ENV;
	}
	if ((uintptr_t)va >= UTOP || ROUNDDOWN((uintptr_t)va, PGSIZE) != (uintptr_t)va) {
		return -E_INVAL;
	}
	if (((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P)) || ((perm & ~(PTE_P | PTE_U | PTE_W | PTE_AVAIL)))) {
		return -E_INVAL;
	}
	struct PageInfo *page = page_alloc(0);
	if (!page) {
		return -E_NO_MEM;
	}
	if (page_insert(env->env_pgdir, page, va, perm)) {
		page_free(page);
		return -E_NO_MEM;
	}
	return 0;
}

// Map the page of memory at 'srcva' in srcenvid's address space
// at 'dstva' in dstenvid's address space with permission 'perm'.
// Perm has the same restrictions as in sys_page_alloc, except
// that it also must not grant write access to a read-only
// page.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if srcenvid and/or dstenvid doesn't currently exist,
//		or the caller doesn't have permission to change one of them.
//	-E_INVAL if srcva >= UTOP or srcva is not page-aligned,
//		or dstva >= UTOP or dstva is not page-aligned.
//	-E_INVAL is srcva is not mapped in srcenvid's address space.
//	-E_INVAL if perm is inappropriate (see sys_page_alloc).
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in srcenvid's
//		address space.
//	-E_NO_MEM if there's no memory to allocate any necessary page tables.
static int
sys_page_map(envid_t srcenvid, void *srcva,
	     envid_t dstenvid, void *dstva, int perm)
{
	struct Env * srcenv, * dstenv;
	int retcode = envid2env(srcenvid, &srcenv, 1);
	retcode |= envid2env(dstenvid, &dstenv, 1);
	if (retcode) {
		return -E_BAD_ENV;
	}
	if ((uintptr_t)srcva >= UTOP
			|| ROUNDDOWN((uintptr_t)srcva, PGSIZE) != (uintptr_t)srcva
			|| (uintptr_t)dstva >= UTOP
			|| ROUNDDOWN((uintptr_t)dstva, PGSIZE) != (uintptr_t)dstva
	) {
		return -E_INVAL;
	}
	pte_t* src_entry;
	struct PageInfo *page = page_lookup(srcenv->env_pgdir, srcva, &src_entry);
	if (!src_entry) {
		return -E_INVAL;
	}
	if (((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P)) || ((perm & ~(PTE_P | PTE_U | PTE_W | PTE_AVAIL)))) {
		return -E_INVAL;
	}
	if ((perm & PTE_W) && !(*src_entry & PTE_W)) {
		return -E_INVAL;
	}
	if (page_insert(dstenv->env_pgdir, page, dstva, perm)) {
		return -E_NO_MEM;
	}
	return 0;
}

// Unmap the page of memory at 'va' in the address space of 'envid'.
// If no page is mapped, the function silently succeeds.
//
// Return 0 on success, < 0 on error.  Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist,
//		or the caller doesn't have permission to change envid.
//	-E_INVAL if va >= UTOP, or va is not page-aligned.
static int
sys_page_unmap(envid_t envid, void *va)
{
	struct Env * env;
	int retcode = envid2env(envid, &env, 1);
	if (retcode) {
		return -E_BAD_ENV;
	}
	if ((uintptr_t)va >= UTOP || ROUNDDOWN((uintptr_t)va, PGSIZE) != (uintptr_t)va) {
		return -E_INVAL;
	}
	page_remove(env->env_pgdir, va);
	return 0;
}

// Try to send 'value' to the target env 'envid'.
// If srcva < UTOP, then also send page currently mapped at 'srcva',
// so that receiver gets a duplicate mapping of the same page.
//
// The send fails with a return value of -E_IPC_NOT_RECV if the
// target is not blocked, waiting for an IPC.
//
// The send also can fail for the other reasons listed below.
//
// Otherwise, the send succeeds, and the target's ipc fields are
// updated as follows:
//    env_ipc_recving is set to 0 to block future sends;
//    env_ipc_from is set to the sending envid;
//    env_ipc_value is set to the 'value' parameter;
//    env_ipc_perm is set to 'perm' if a page was transferred, 0 otherwise.
// The target environment is marked runnable again, returning 0
// from the paused sys_ipc_recv system call.  (Hint: does the
// sys_ipc_recv function ever actually return?)
//
// If the sender wants to send a page but the receiver isn't asking for one,
// then no page mapping is transferred, but no error occurs.
// The ipc only happens when no errors occur.
//
// Returns 0 on success, < 0 on error.
// Errors are:
//	-E_BAD_ENV if environment envid doesn't currently exist.
//		(No need to check permissions.)
//	-E_IPC_NOT_RECV if envid is not currently blocked in sys_ipc_recv,
//		or another environment managed to send first.
//	-E_INVAL if srcva < UTOP but srcva is not page-aligned.
//	-E_INVAL if srcva < UTOP and perm is inappropriate
//		(see sys_page_alloc).
//	-E_INVAL if srcva < UTOP but srcva is not mapped in the caller's
//		address space.
//	-E_INVAL if (perm & PTE_W), but srcva is read-only in the
//		current environment's address space.
//	-E_NO_MEM if there's not enough memory to map srcva in envid's
//		address space.
static int
sys_ipc_try_send(envid_t envid, uint32_t value, void *srcva, unsigned perm)
{	
	struct Env * recv_env;
	if (envid2env(envid, &recv_env, 0))
		return -E_BAD_ENV;
	if (!recv_env->env_ipc_recving)
		return -E_IPC_NOT_RECV;
	if (recv_env->env_ipc_from != 0 && recv_env->env_ipc_from != curenv->env_id)	
		return -E_IPC_NOT_RECV;
	if ((uintptr_t) srcva < UTOP){
		if ((uintptr_t) srcva != ROUNDDOWN((uintptr_t) srcva, PGSIZE))
			return -E_INVAL;
		if (((perm & (PTE_U | PTE_P)) != (PTE_U | PTE_P)) || 
		    ((perm & ~(PTE_P | PTE_U | PTE_W | PTE_AVAIL)))) 
			return -E_INVAL;
		pte_t* src_entry;
		struct PageInfo *page = page_lookup(curenv->env_pgdir, srcva, &src_entry);
		if (!src_entry)
			return -E_INVAL;
		if (!(*src_entry & PTE_P))
			return -E_INVAL;
		if (!(*src_entry & PTE_W) && (PTE_W & perm))
			return -E_INVAL;
		if ((uintptr_t) recv_env->env_ipc_dstva < UTOP){
			if (page_insert(recv_env->env_pgdir, page, 
				recv_env->env_ipc_dstva, perm)) {
				return -E_NO_MEM;
			}
		}
	}	
	recv_env->env_ipc_recving = 0;
	recv_env->env_ipc_from = curenv->env_id;
	recv_env->env_ipc_value = value;
	recv_env->env_status = ENV_RUNNABLE;
	recv_env->env_ipc_perm = ((uintptr_t) srcva < UTOP && 
				  (uintptr_t) recv_env->env_ipc_dstva < UTOP) ? 
				 	perm : 0;
	return 0;
}

// Block until a value is ready.  Record that you want to receive
// using the env_ipc_recving and env_ipc_dstva fields of struct Env,
// mark yourself not runnable, and then give up the CPU.
//
// If 'dstva' is < UTOP, then you are willing to receive a page of data.
// 'dstva' is the virtual address at which the sent page should be mapped.
//
// This function only returns on error, but the system call will eventually
// return 0 on success.
// Return < 0 on error.  Errors are:
//	-E_INVAL if dstva < UTOP but dstva is not page-aligned.
static int
sys_ipc_recv(void *dstva)
{	
	if ((uintptr_t) dstva < UTOP && 
	    (uintptr_t) dstva != ROUNDDOWN((uintptr_t) dstva, PGSIZE))
		return -E_INVAL;

	curenv->env_ipc_dstva = dstva;
	curenv->env_ipc_recving = 1;
	curenv->env_status = ENV_NOT_RUNNABLE;
	curenv->env_tf.tf_regs.reg_eax = 0;
	curenv->env_ipc_from = 0;
	sched_yield();
}

static int
sys_ipc_recv_from(envid_t fromenv, void *dstva)
{	
	if ((uintptr_t) dstva < UTOP && 
	    (uintptr_t) dstva != ROUNDDOWN((uintptr_t) dstva, PGSIZE))
		return -E_INVAL;

	curenv->env_ipc_dstva = dstva;
	curenv->env_ipc_recving = 1;
	curenv->env_status = ENV_NOT_RUNNABLE;
	curenv->env_tf.tf_regs.reg_eax = 0;
	curenv->env_ipc_from = fromenv;
	sched_yield();
}

int32_t
sysenter_syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	int retval;
	lock_kernel();
	retval = syscall(syscallno, a1, a2, a3, a4, a5);
	unlock_kernel();
	return retval;
}

// Dispatches to the correct kernel function, passing the arguments.
int32_t
syscall(uint32_t syscallno, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5)
{
	// Call the function corresponding to the 'syscallno' parameter.
	// Return any appropriate return value.
	switch (syscallno) {
	case SYS_cputs:
		sys_cputs((char *)a1, a2);
		return 0;
	case SYS_cgetc:
		return sys_cgetc();
	case SYS_getenvid:
		return sys_getenvid();
	case SYS_env_destroy:
		return sys_env_destroy(a1);
	case SYS_ipc_recv:
		return sys_ipc_recv((void *) a1);
	case SYS_ipc_recv_from:
		return sys_ipc_recv_from(a1, (void *) a2);
	case SYS_ipc_try_send:
		return sys_ipc_try_send(a1, a2, (void *) a3, a4);
	case SYS_yield:
		sys_yield();
		return 0;
	case SYS_exofork:
		return sys_exofork();
	case SYS_env_set_status:
		return sys_env_set_status(a1, a2);
	case SYS_page_alloc:
		return sys_page_alloc(a1, (void *) a2, a3);
	case SYS_page_map:
		return sys_page_map(a1, (void *) a2, a3, (void *) a4, a5);
	case SYS_page_unmap:
		return sys_page_unmap(a1, (void *) a2);
	case SYS_env_set_pgfault_upcall:
		return sys_env_set_pgfault_upcall(a1, (void *) a2);
	default:
		return -E_INVAL;
	}
}

