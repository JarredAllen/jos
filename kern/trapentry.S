/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>



###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

TRAPHANDLER_NOEC(Trap_Divide, T_DIVIDE)
TRAPHANDLER_NOEC(Trap_Debug, T_DEBUG)
TRAPHANDLER_NOEC(Trap_Nmi, T_NMI)
TRAPHANDLER_NOEC(Trap_Brkpt, T_BRKPT)
TRAPHANDLER_NOEC(Trap_Oflow, T_OFLOW)
TRAPHANDLER_NOEC(Trap_Bound, T_BOUND)
TRAPHANDLER_NOEC(Trap_Illop, T_ILLOP)
TRAPHANDLER_NOEC(Trap_Device, T_DEVICE)
TRAPHANDLER     (Trap_Dblflt, T_DBLFLT)
TRAPHANDLER     (Trap_Tss, T_TSS)
TRAPHANDLER     (Trap_Segnp, T_SEGNP)
TRAPHANDLER     (Trap_Stack, T_STACK)
TRAPHANDLER     (Trap_Gpflt, T_GPFLT)
TRAPHANDLER     (Trap_Pgflt, T_PGFLT)
TRAPHANDLER_NOEC(Trap_Fperr, T_FPERR)
TRAPHANDLER     (Trap_Align, T_ALIGN)
TRAPHANDLER_NOEC(Trap_Mchk, T_MCHK)
TRAPHANDLER_NOEC(Trap_Simderr, T_SIMDERR)

TRAPHANDLER_NOEC(Trap_Syscall, T_SYSCALL)
TRAPHANDLER_NOEC(Trap_Default, T_DEFAULT)


/*
 * Lab 3: Your code here for _alltraps
 */
	_alltraps:
	pushl %ds
	pushl %es
	pushal
	mov $GD_KD, %eax
	mov %eax, %ds
	mov %eax, %es
	pushl %esp
	call trap

