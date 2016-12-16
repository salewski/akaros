/* Copyright (c) 2014 The Regents of the University of California
 * Barret Rhoden <brho@cs.berkeley.edu>
 * See LICENSE for details.
 */


#include <stdlib.h>
#include <stdio.h>
#include <parlib/parlib.h>
#include <unistd.h>
#include <parlib/event.h>
#include <benchutil/measure.h>
#include <parlib/uthread.h>
#include <parlib/timing.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <iplib/iplib.h>

int main()
{
	int ret;
	int afd, dfd, lcfd;
	char adir[40], ldir[40];
	int n;
	uint8_t buf[1500]; // XXX MTU

	/* This clones a conversation (opens /net/tcp/clone), then reads the cloned
	 * fd (which is the ctl) to givure out the conv number (the line), then
	 * writes "announce [addr]" into ctl.  This "announce" command often has a
	 * "bind" in it too.  plan9 bind just sets the local addr/port.  TCP
	 * announce also does this.  Returns the ctlfd. */
	afd = bypass9("udp!*!2345", adir, 0);

	if (afd < 0) {
		perror("Bypass failure");
		return -1;
	}
	printf("Bypassing on line %s\n", adir);


	dfd = open_data_fd9(adir);

	/* echo until EOF */
	printf("Server read: ");
	while ((n = read(dfd, buf, sizeof(buf))) > 0) {
		printf("\nGot packet at %p, len %d:\n", buf, n);
		hexdump(stdout, buf, n);
		buf[15] = 0x64; // src 100 0x64
		buf[19] = 0x45; // dst 69 0x45
		fflush(stdout);
		write(dfd, buf, buf[3]);
	}

	close(dfd);
	close(afd);
}

