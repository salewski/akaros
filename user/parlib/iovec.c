/* Copyright (c) 2016 Google Inc
 * Barret Rhoden <brho@cs.berkeley.edu>
 * See LICENSE for details.
 *
 * Various utility functions. */

#include <parlib/iovec.h>
#include <assert.h>

/* Strips bytes from the front of the iovec.  This modifies the base and
 * length of the iovecs in the array. */
void iov_strip_bytes(struct iovec *iov, int iovcnt, size_t amt)
{
	for (int i = 0; i < iovcnt; i++) {
		if (iov[i].iov_len <= amt) {
			amt -= iov[i].iov_len;
			iov[i].iov_len = 0;
			continue;
		}
		iov[i].iov_len -= amt;
		iov[i].iov_base += amt;
		amt = 0;
	}
}
