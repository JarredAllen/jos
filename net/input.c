#include "ns.h"

extern union Nsipc nsipcbuf;
struct ns_input_packet recv_data __attribute__((aligned (PGSIZE)));

void
input(envid_t ns_envid)
{
	binaryname = "ns_input";

	int rcvlen;
	cprintf("Buffer addr: %x\n", &recv_data);
	while (1){
		while ((rcvlen = sys_e1000_recv(&recv_data)) < 0){
			sys_yield();
		}
		cprintf("receiving packet, len = %d\n", rcvlen);
		recv_data.len = rcvlen;
		ipc_send(ns_envid, NSREQ_INPUT, &recv_data, PTE_U | PTE_W | PTE_P);
	}
}
