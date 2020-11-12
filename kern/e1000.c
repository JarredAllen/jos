#include <kern/e1000.h>
#include <kern/pmap.h>
#include <kern/pci.h>
#include <inc/error.h>

// LAB 6: Your driver code here

void * e1000_BAR0;
struct tx_desc * transmit_buf;

#define E_REG(offset) (*(uint32_t *) (e1000_BAR0 + (offset)))

int
attach_e1000(struct pci_func * pcif)
{
	pci_func_enable(pcif);
	e1000_BAR0 = mmio_map_region(pcif->reg_base[0],pcif->reg_size[0]);
	cprintf("e1000 status: 0x%x\n", E_REG(E1000_STATUS));
	struct PageInfo * buf_pg = page_alloc(ALLOC_ZERO);
	transmit_buf = page2kva(buf_pg);
	E_REG(E1000_TDBAL) = page2pa(buf_pg);
	E_REG(E1000_TDLEN) = E1000_TBUFSIZE;
	E_REG(E1000_TDH)   = 0;
	E_REG(E1000_TDT)   = 0;
	E_REG(E1000_TCTL)  = E1000_TCTL_EN 
			   | E1000_TCTL_PSP 
			   | (E1000_TCTL_CT & 0x100) 
			   | (E1000_TCTL_COLD & 0x40000);
	E_REG(E1000_TIPG)  = 10 | (8 << 10) | (6 << 20); 
	for (int i = 0; i < E1000_TBUFCNT; i++) {
		transmit_buf[i].status = E1000_TXSTAT_DD;
	}
	send_data(0, 0, 1);
	return 0;
}

int
send_data(void * start, int len, int eop)
{
	#define tail E_REG(E1000_TDT)
	if (!(transmit_buf[tail].status & E1000_TXSTAT_DD)) {
		return -E_NO_MEM;
	}
	transmit_buf[tail] = (struct tx_desc) {
		.addr   = (uint32_t) start, 
		.length = (uint16_t) len, 
		.cmd = E1000_TXCMD_RS | eop, 
	};
	tail = (tail + 1) % E1000_TBUFCNT;
	#undef tail
	return 0;
}
