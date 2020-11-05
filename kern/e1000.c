#include <kern/e1000.h>
#include <kern/pmap.h>
#include <kern/pci.h>

// LAB 6: Your driver code here

void * e1000_BAR0;

#define E_REG(offset) (*(int *) (e1000_BAR0 + (offset)))

int
attach_e1000(struct pci_func * pcif)
{
	pci_func_enable(pcif);
	e1000_BAR0 = mmio_map_region(pcif->reg_base[0],pcif->reg_size[0]);
	cprintf("e1000 status: 0x%x\n", E_REG(E1000_STATUS));
	return 0;
}
