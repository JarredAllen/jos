
#include <inc/string.h>
#include <inc/lib.h>

uint32_t rand_state = 0xdeadcafe;

float rand() {
	rand_state ^= rand_state << 13;
	rand_state ^= rand_state >> 17;
	rand_state ^= rand_state << 5;
	return ((float)rand_state / 4294967296.0);
}

float monte_carlo_pi_approx(int nrounds) {
	float inside = 0.0;
	float outside = 0.0;
	float x,y;
	for (int i=0; i < nrounds; i++) {
		x = rand();
		y = rand();
		if (x*x + y*y <= 1) {
			inside += 1.0;
		} else {
			outside += 1.0;
		}
		sys_yield();
	}
	return inside / (inside + outside);
}

envid_t dumbfork(void);
static void print_float(float);

void umain(int argc, char** argv) {
	for (int i=0; i < 4; i++) {
		if (dumbfork() == 0) {
			cprintf("Child number %d\n", i);
			rand_state ^= 65537 * i;
			float pi_approx = monte_carlo_pi_approx(100+5*i)*4;
			cprintf("[%d] Ended with pi=", i);
			print_float(pi_approx);
			cprintf("\n");
			return;
		}
	}
}

static void
duppage(envid_t dstenv, void *addr)
{
	int r;

	// This is NOT what you should do in your fork.
	if ((r = sys_page_alloc(dstenv, addr, PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_alloc: %e", r);
	if ((r = sys_page_map(dstenv, addr, 0, UTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		panic("sys_page_map: %e", r);
	memmove(UTEMP, addr, PGSIZE);
	if ((r = sys_page_unmap(0, UTEMP)) < 0)
		panic("sys_page_unmap: %e", r);
}

envid_t
dumbfork(void)
{
	envid_t envid;
	uint8_t *addr;
	int r;
	extern unsigned char end[];

	// Allocate a new child environment.
	// The kernel will initialize it with a copy of our register state,
	// so that the child will appear to have called sys_exofork() too -
	// except that in the child, this "fake" call to sys_exofork()
	// will return 0 instead of the envid of the child.
	envid = sys_exofork();
	if (envid < 0)
		panic("sys_exofork: %e", envid);
	if (envid == 0) {
		// We're the child.
		// The copied value of the global variable 'thisenv'
		// is no longer valid (it refers to the parent!).
		// Fix it and return 0.
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	// We're the parent.
	// Eagerly copy our entire address space into the child.
	// This is NOT what you should do in your fork implementation.
	for (addr = (uint8_t*) UTEXT; addr < end; addr += PGSIZE)
		duppage(envid, addr);

	// Also copy the stack we are currently running on.
	duppage(envid, ROUNDDOWN(&addr, PGSIZE));

	// Start the child environment running
	if ((r = sys_env_set_status(envid, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);

	return envid;
}

static void print_float_integer_part(float value) {
	if (value < 1.0) {
		return;
	}
	int digit = (int)value % 10;
	print_float_integer_part(value / 10.0);
	cprintf("%d", digit);
}

static void print_float(float value) {
	if (value == 0.0) {
		cprintf("0");
		return;
	}
	if (value < 0.0) {
		cprintf("-");
		value *= -1.0;
	}
	if (value < 1.0) {
		cprintf("0");
	} else {
		print_float_integer_part(value);
	}
	value = value - (int)value;
	if (value != 0.0) {
		cprintf(".");
		while (value > 0.0) {
			value *= 10.0;
			cprintf("%d", (int)value);
			value = value - (int)value;
		}
	}
}
