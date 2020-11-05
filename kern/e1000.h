#ifndef JOS_KERN_E1000_H

#include <kern/pci.h>

#define E1000_STATUS   0x00008  /* Device Status - RO */

#define E1000_VND_ID	0x8086	/* Vendor ID */
#define E1000_DEV_ID	0x100E	/* Device ID */


int attach_e1000(struct pci_func * pcif);

#define JOS_KERN_E1000_H
#endif  // SOL >= 6
