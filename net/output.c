#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";
	
	while(1) {
		ipc_recv(0, &nsipcbuf, 0);
		while (sys_send_packet(&nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len) < 0){
			sys_yield();
		}
	}
}
