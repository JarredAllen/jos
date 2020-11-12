#include <kern/e1000.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/pci.h>
#include <inc/error.h>
#include <inc/string.h>

// LAB 6: Your driver code here

void * e1000_BAR0;
struct e1000_tx_desc * transmit_buf;
struct e1000_rx_desc * recv_buf;

#define E_REG(offset) (*(uint32_t *) (e1000_BAR0 + (offset)))

int
attach_e1000(struct pci_func * pcif)
{
	pci_func_enable(pcif);
	e1000_BAR0 = mmio_map_region(pcif->reg_base[0],pcif->reg_size[0]);
	cprintf("e1000 status: 0x%x\n", E_REG(E1000_STATUS));
	struct PageInfo * buf_pg = page_alloc(ALLOC_ZERO);

	// Initialize transmit:
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

	// Initialize Receive:
	E_REG(E1000_RAL) = 0x52540012;
	E_REG(E1000_RAH) = 0x3456;
	memset(e1000_BAR0 + E1000_MTA, 0, 512);
	E_REG(E1000_IMS) = 0;
	E_REG(E1000_RDTR) = 0;
	recv_buf = page2kva(buf_pg) + 2048;
	E_REG(E1000_RDBAL) = PADDR(recv_buf);
	E_REG(E1000_RDLEN) = E1000_RBUFSIZE;
	E_REG(E1000_RCTL)  = E1000_RCTL_EN 
			   | E1000_RCTL_LBM_NO 
			   | E1000_RCTL_BAM 
			   | E1000_RCTL_SECRC;
	return 0;
}

int
send_data(void * start, int len, int eop)
{
	#define tail E_REG(E1000_TDT)
	if (!(transmit_buf[tail].status & E1000_TXSTAT_DD)) {
		return -E_NO_MEM;
	}
	uint32_t pstart = ((* pgdir_walk(curenv->env_pgdir, start, 0)) 
				& (~0xfff)) 
			  + PGOFF(start);
	transmit_buf[tail] = (struct e1000_tx_desc) {
		.addr   = pstart, 
		.length = (uint16_t) len, 
		.cmd = E1000_TXCMD_RS | eop, 
	};
	tail = (tail + 1) % E1000_TBUFCNT;
	#undef tail
	return 0;
}
