#include "ns.h"

extern union Nsipc nsipcbuf;
char recv_data[PGSIZE] __attribute__((aligned (PGSIZE)));

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	// LAB 6: Your code here:
	// 	- read a packet from the device driver
	//	- send it to the network server
	// Hint: When you IPC a page to the network server, it will be
	// reading from it for a while, so don't immediately receive
	// another packet in to the same physical page.
	int rcvlen;
	cprintf("Buffer addr: %x\n", &recv_data);
	//cprintf("envid: %x\n", thisenv);
	while (1){
		while ((rcvlen = sys_e1000_recv(&recv_data)) < 0){
			sys_yield();
		}
		cprintf("receiving packet, len = %d\n", rcvlen);
		* (int *) &recv_data[2048] = rcvlen;
		ipc_send(ns_envid, NSREQ_INPUT, &recv_data, PTE_U | PTE_W | PTE_P);
	}
}
