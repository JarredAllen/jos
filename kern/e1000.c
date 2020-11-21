#include <kern/e1000.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/pci.h>
#include <inc/error.h>
#include <inc/string.h>

void * e1000_BAR0;
struct e1000_tx_desc * transmit_buf;
struct e1000_rx_desc * recv_buf;

static void read_mac_address();

#define E_REG(offset) (*(volatile uint32_t *) (e1000_BAR0 + (offset)))

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
	read_mac_address();
	E_REG(E1000_RAL) =
		((uint32_t)e1000_mac_address[0] << 0)
		| ((uint32_t)e1000_mac_address[1] << 8)
		| ((uint32_t)e1000_mac_address[2] << 16)
		| ((uint32_t)e1000_mac_address[3] << 24);
	E_REG(E1000_RAH) = 0x80000000
		| ((uint32_t)e1000_mac_address[4] << 0)
		| ((uint32_t)e1000_mac_address[5] << 8);
	memset((uint32_t *) &E_REG(E1000_MTA), 0, 512);
	E_REG(E1000_IMS) = 0;
	E_REG(E1000_RDTR) = 0;
	recv_buf = page2kva(buf_pg) + 2048;
	E_REG(E1000_RDBAL) = PADDR(recv_buf);
	E_REG(E1000_RDLEN) = E1000_RBUFSIZE;
	E_REG(E1000_RCTL)  = E1000_RCTL_EN 
			   | E1000_RCTL_LBM_NO 
			   | E1000_RCTL_BAM 
			   | E1000_RCTL_SECRC;
	E_REG(E1000_RDH) = 0;
	E_REG(E1000_RDT) = E1000_RBUFCNT - 1;
	for(int i = 0; i < E1000_RBUFCNT; i++){
		recv_buf[i].buffer_addr = (uintptr_t) page2pa(page_alloc(ALLOC_ZERO));
	}
	return 0;
}

int
send_data(void * start, int len, int eop)
{
	#define tail E_REG(E1000_TDT)
	if (!(transmit_buf[tail].status & E1000_TXSTAT_DD)) {
		return -E_NO_MEM;
	}
	uint32_t pstart = PTE_ADDR(* pgdir_walk(curenv->env_pgdir, start, 0))
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


// If data is successfully received, returns the length and maps the received data at va
// If no packet is available, returns -E_NO_MEM
// This function does no checking to ensure that the va is valid
int 
recv_data(void * va)
{
	#define tail (E_REG(E1000_RDT))
	#define newtail ((E_REG(E1000_RDT) + 1) % E1000_RBUFCNT)
	if (recv_buf[newtail].status & E1000_RXSTAT_DD){
		page_insert(curenv->env_pgdir, 
			    pa2page((physaddr_t) recv_buf[newtail].buffer_addr), 
			    va,
			    PTE_U | PTE_W
			   );
		recv_buf[newtail].status = 0;
		recv_buf[newtail].buffer_addr = 
			(uintptr_t) page2pa(page_alloc(ALLOC_ZERO));
		int length = recv_buf[newtail].length;
		tail = newtail;
		return length; 
	}
	return -E_NO_MEM;
	#undef tail
	#undef newtail
}

// Read a word from the eeprom at the specified address
static uint16_t
read_eeprom(uint8_t addr)
{
	E_REG(E1000_EERD) = E1000_EERD_START | (addr << E1000_EERD_ADDR_SHIFT);
	while (!(E_REG(E1000_EERD) & E1000_EERD_DONE))
		;
	uint16_t result = (uint16_t) (E_REG(E1000_EERD) >> E1000_EERD_DATA_SHIFT);
	return result;
}

// Read the MAC address from the E1000 and put it in the global variable
static void
read_mac_address()
{
	uint16_t t;
	t = read_eeprom(0);
	e1000_mac_address[0] = t & 0xFF;
	e1000_mac_address[1] = (t >> 8) & 0xFF;
	t = read_eeprom(1);
	e1000_mac_address[2] = t & 0xFF;
	e1000_mac_address[3] = (t >> 8) & 0xFF;
	t = read_eeprom(2);
	e1000_mac_address[4] = t & 0xFF;
	e1000_mac_address[5] = (t >> 8) & 0xFF;
}
