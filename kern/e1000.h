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
#define E1000_TBUFCNT  (E1000_TBUFSIZE/sizeof(struct e1000_tx_desc)) 
/* Size of Buffer Array */

#define E1000_TIPG     0x00410  /* TX Inter-packet gap -RW */

#define E1000_TCTL     0x00400  /* TX Control - RW */

#define E1000_TCTL_EN     0x00000002    /* enable tx */
#define E1000_TCTL_PSP    0x00000008    /* pad short packets */
#define E1000_TCTL_CT     0x00000ff0    /* collision threshold */
#define E1000_TCTL_COLD   0x003ff000    /* collision distance */

#define E1000_TXCMD_RS  8
#define E1000_TXSTAT_DD 1

#define E1000_RAL      0x05400  /* Receive Address Low - RW Array */
#define E1000_RAH      0x05404  /* Receive Address High - RW Array */

#define E1000_MTA      0x05200  /* Multicast Table Array - RW Array */

#define E1000_IMS      0x000D0  /* Interrupt Mask Set - RW */

#define E1000_RCTL     0x00100  /* RX Control - RW */
#define E1000_RDBAL    0x02800  /* RX Descriptor Base Address Low - RW */
#define E1000_RDLEN    0x02808  /* RX Descriptor Length - RW */
#define E1000_RDH      0x02810  /* RX Descriptor Head - RW */
#define E1000_RDT      0x02818  /* RX Descriptor Tail - RW */
#define E1000_RDTR     0x02820  /* RX Delay Timer - RW */

#define E1000_RCTL_EN             0x00000002    /* enable */
#define E1000_RCTL_LPE            0x00000020    /* long packet enable */
#define E1000_RCTL_LBM_NO         0x00000000    /* no loopback mode */
#define E1000_RCTL_BAM            0x00008000    /* broadcast enable */
#define E1000_RCTL_SZ_2048        0x00000000    /* rx buffer size 2048 */
#define E1000_RCTL_SECRC          0x04000000    /* Strip Ethernet CRC */

#define E1000_EERD     0x00014  /* EEPROM Read - RW */

#define E1000_EERD_START          0x00000001    /* start read */
#define E1000_EERD_DONE           0x00000010    /* read done */
#define E1000_EERD_ADDR_SHIFT     8             /* addr field shift amount */
#define E1000_EERD_DATA_SHIFT     16            /* data field shift amount */

#define E1000_RBUFSIZE 1024	/* Receive Buffer Size */
#define E1000_RBUFCNT  (E1000_RBUFSIZE/sizeof(struct e1000_rx_desc)) 
/* Size of Buffer Array */

/* Transmit Descriptor */
struct e1000_tx_desc
{
	uint64_t addr;
	uint16_t length;
	uint8_t cso;
	uint8_t cmd;
	uint8_t status;
	uint8_t css;
	uint16_t special;
};

/* Receive Descriptor */
struct e1000_rx_desc {
	uint64_t buffer_addr;	/* Address of the descriptor's data buffer */
	uint16_t length;	/* Length of data DMAed into data buffer */
	uint16_t csum;		/* Packet checksum */
	uint8_t status;		/* Descriptor status */
	uint8_t errors;      	/* Descriptor Errors */
	uint16_t special;
};

// A global which is initialized to the mac address when the e1000 is attached.
uint64_t e1000_mac_address;

int attach_e1000(struct pci_func * pcif);
int send_data(void * start, int len, int eop);

#define JOS_KERN_E1000_H
#endif  // SOL >= 6
