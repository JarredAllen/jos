#ifndef JOS_KERN_E1000_H

#include <kern/pci.h>

#define E1000_VND_ID	0x8086	/* Vendor ID */
#define E1000_DEV_ID	0x100E	/* Device ID */

#define E1000_STATUS   0x00008  /* Device Status - RO */

#define E1000_TDBAL    0x03800  /* TX Descriptor Base Address Low - RW */
#define E1000_TDLEN    0x03808  /* TX Descriptor Length - RW */
#define E1000_TDH      0x03810  /* TX Descriptor Head - RW */
#define E1000_TDT      0x03818  /* TX Descriptor Tail - RW */

#define E1000_TBUFSIZE 1024	/* Transmit Buffer Size */
#define E1000_TBUFCNT  (E1000_TBUFSIZE/sizeof(struct tx_desc)) /* Size of Buffer Array */

#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */

#define E1000_TCTL     0x00400  /* TX Control - RW */

#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */

#define E1000_TXCMD_RS  8
#define E1000_TXSTAT_DD 1

struct tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};

int attach_e1000(struct pci_func * pcif);
int send_data(void * start, int len, int eop);

#define JOS_KERN_E1000_H
#endif  // SOL >= 6
