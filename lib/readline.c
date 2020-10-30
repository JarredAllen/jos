#include <inc/stdio.h>
#include <inc/error.h>

#define BUFLEN 1024
static char buf[BUFLEN];

char *
readline(const char *prompt)
{
	int i, c, echoing;

#if JOS_KERNEL
	if (prompt != NULL)
		cprintf("%s", prompt);
#else
	if (prompt != NULL)
		fprintf(1, "%s", prompt);
#endif

	i = 0;
	echoing = iscons(0);
	while (1) {
		c = getchar();
		if (c < 0) {
			if (c != -E_EOF)
				cprintf("read error: %e\n", c);
			return NULL;
		} else if (c == '\b' || c == '\x7f') {
			if (i > 0) {
				if (echoing) {
					cputchar('\b');
					cputchar(' ');
					cputchar('\b');
				}
				i--;
			}
		} else if (c >= ' ' && i < BUFLEN-1) {
			if (echoing)
				cputchar(c);
			buf[i++] = c;
		} else if (c == '\n' || c == '\r') {
			if (echoing)
				cputchar('\n');
			buf[i] = 0;
			return buf;
		} else if (c == '\033') {
			c = getchar();
			if (c == '[') {
				c = getchar();
				switch (c) {
				case 'D':
					if (i > 0) {
						i--;
						cputchar('\b');
					}
				break;
				case 'C':
					if (buf[i]) {
						cputchar(buf[i++]);
					}
				break;
				case 'A':
					while (i--) {
						cputchar('\b');
						cputchar(' ');
						cputchar('\b');
					}
					while (buf[++i]) {
						cputchar(buf[i]);
					}
				break;
				case 'B':
					while (i--) {
						cputchar('\b');
						cputchar(' ');
						cputchar('\b');
					}
					i = 0;
				break;
				}
			}
		}
	}
}

