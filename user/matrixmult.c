#include <inc/lib.h>
#include <inc/string.h>

#define WIDTH 5
#define HEIGHT 5
#define SIZE (WIDTH*HEIGHT)

volatile envid_t children[25];
volatile int notready;

//zeroes...
void
send_zeroes(int pos)
{	
	while(1){
		ipc_send(children[pos+5], 0, NULL, 0);
	}
}

//input...
void
send_input(int* vectors, int pos)
{
	for (int i = 0; i < 4; i++) {
		ipc_send(children[pos+1], vectors[i], NULL, 0);
	}
}
//compute...
void
compute(int val, int pos)
{
	envid_t north = children[pos - 5];
	envid_t west  = children[pos - 1];
	envid_t east  = children[pos + 1];
	envid_t south = children[pos + 5];
	int above, left;

	while (1) {
		above = ipc_recv_from(north, NULL, NULL);
		left  = ipc_recv_from(west, NULL, NULL);
		above += val*left;
		ipc_send(south, above, NULL, 0);
		ipc_send(east, left, NULL, 0);
	}
}

//sink...
void 
sink()
{
	while (1){
		ipc_recv(NULL,NULL,NULL);
	}
}

//output...
void
recv_output(int pos)
{
	int r;
	for (int i = 0;;i++) {
		r = ipc_recv(NULL,NULL,NULL);
		cprintf("Returned %d in component %d, vector #%d\n", r, (pos % 5), i);
	}
}

void
umain(int argc, char** argv)
{	
	int x[4] = {0,1,1,3};
	int y[4] = {0,1,2,2};
	int z[4] = {0,1,3,1};
	envid_t pos;
	notready = 1;
	for (int i = 1; i < SIZE-1; i++) {
		if (i == 4 || i == 20) { // Unused corners
			continue;
		}
		if ((pos = sfork()) == 0) {
			while (notready){
				sys_yield();
			}
			if (i < 5) {
				send_zeroes(i);
			}
			else if (i % 5 == 0) {
				switch (i) {
					case 5:
						send_input(x, i);
						break;
					case 10:
						send_input(y, i);
						break;
					case 15:
						send_input(z, i);
						break;
				}
			}
			else if (i % 5 == 4) {
				sink();
			}
			else if (i > 20) {
				recv_output(i);
			}
			else {
				compute(i, i);
			}
			return;
		} else {
			children[i] = pos;
		}

	}
	notready = 0;
	for (int i = 0; i < 1000; i++) {
		sys_yield();
	}
	for (int child = 1; child < SIZE - 1; child++) {
		if (child == 4 || child == 20) { // Unused corners
			continue;
		}
		sys_env_destroy(children[child]);
	}
}
