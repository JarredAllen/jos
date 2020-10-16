/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


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
#define TRAPHANDLER(name, num, istrap, dpl)				\
	.text;								\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps;							\
	.data;								\
	.long num;							\
	.long istrap;						\
	.long name;							\
	.long dpl

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num, istrap, dpl)			\
	.text;								\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps;							\
	.data;								\
	.long num;								\
	.long istrap;							\
	.long name;							\
	.long dpl

.data
	.p2align	PGSHIFT		# force page alignment
	.long 0
	.globl traphandler_data
traphandler_data:
.text

TRAPHANDLER_NOEC(Trap_Divide, T_DIVIDE, 1, 0)
TRAPHANDLER_NOEC(Trap_Debug, T_DEBUG, 1, 0)
TRAPHANDLER_NOEC(Trap_Nmi, T_NMI, 0, 0)
TRAPHANDLER_NOEC(Trap_Brkpt, T_BRKPT, 1, 3)
TRAPHANDLER_NOEC(Trap_Oflow, T_OFLOW, 1, 0)
TRAPHANDLER_NOEC(Trap_Bound, T_BOUND, 1, 0)
TRAPHANDLER_NOEC(Trap_Illop, T_ILLOP, 1, 0)
TRAPHANDLER_NOEC(Trap_Device, T_DEVICE, 1, 0)
TRAPHANDLER     (Trap_Dblflt, T_DBLFLT, 1, 0)
TRAPHANDLER     (Trap_Tss, T_TSS, 1, 0)
TRAPHANDLER     (Trap_Segnp, T_SEGNP, 1, 0)
TRAPHANDLER     (Trap_Stack, T_STACK, 1, 0)
TRAPHANDLER     (Trap_Gpflt, T_GPFLT, 1, 0)
TRAPHANDLER     (Trap_Pgflt, T_PGFLT, 1, 0)
TRAPHANDLER_NOEC(Trap_Fperr, T_FPERR, 1, 0)
TRAPHANDLER     (Trap_Align, T_ALIGN, 1, 0)
TRAPHANDLER_NOEC(Trap_Mchk, T_MCHK, 1, 0)
TRAPHANDLER_NOEC(Trap_Simderr, T_SIMDERR, 1, 0)

TRAPHANDLER_NOEC(Irq_Timer,    IRQ_OFFSET + IRQ_TIMER,    0, 0)
TRAPHANDLER_NOEC(Irq_Kbd,      IRQ_OFFSET + IRQ_KBD,      0, 0)
TRAPHANDLER_NOEC(Irq_Two,      IRQ_OFFSET + 2,            0, 0)
TRAPHANDLER_NOEC(Irq_Three,    IRQ_OFFSET + 3,            0, 0)
TRAPHANDLER_NOEC(Irq_Serial,   IRQ_OFFSET + IRQ_SERIAL,   0, 0) 
TRAPHANDLER_NOEC(Irq_Five,     IRQ_OFFSET + 5,            0, 0)
TRAPHANDLER_NOEC(Irq_Six,      IRQ_OFFSET + 6,            0, 0)
TRAPHANDLER_NOEC(Irq_Spurious, IRQ_OFFSET + IRQ_SPURIOUS, 0, 0)
TRAPHANDLER_NOEC(Irq_Eight,    IRQ_OFFSET + 8,            0, 0)
TRAPHANDLER_NOEC(Irq_Nine,     IRQ_OFFSET + 9,            0, 0)
TRAPHANDLER_NOEC(Irq_Ten,      IRQ_OFFSET + 10,           0, 0)
TRAPHANDLER_NOEC(Irq_Eleven,   IRQ_OFFSET + 11,           0, 0)
TRAPHANDLER_NOEC(Irq_Twelve,   IRQ_OFFSET + 12,           0, 0)
TRAPHANDLER_NOEC(Irq_Thirteen, IRQ_OFFSET + 13,           0, 0)
TRAPHANDLER_NOEC(Irq_Ide,      IRQ_OFFSET + IRQ_IDE,      0, 0)
TRAPHANDLER_NOEC(Irq_Fifteen,  IRQ_OFFSET + 15,           0, 0)

TRAPHANDLER_NOEC(Trap_Syscall, T_SYSCALL, 0, 3)
TRAPHANDLER_NOEC(Trap_Default, T_DEFAULT, 0, 0)

.data
.globl end_traphandler_data
end_traphandler_data:

/*
 * Lab 3: Your code here for _alltraps
 */
.text
	_alltraps:
	pushl %ds
	pushl %es
	pushal
	movl %esp, %eax
	subl $512, %eax
	andl $0xFFFFFFF0, %eax
	fxsave (%eax)
	mov $GD_KD, %eax
	mov %eax, %ds
	mov %eax, %es
	pushl %esp
	call trap
	// This section can't be reached because trap ends by switching
	// back into the environment, not by returning.


.globl sysenter_handler
sysenter_handler:
	pushl $0
	pushl %edi
	pushl %ebx
	pushl %ecx
	pushl %edx
	pushl %eax
	call sysenter_syscall
	movl %esi, %edx
	movl %ebp, %ecx
	sysexit
