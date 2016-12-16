/* Copyright (c) 2016 Google Inc
 * Barret Rhoden <brho@cs.berkeley.edu>
 * See LICENSE for details.
 *
 * Network address translation for VM guests */

#pragma once

#include <sys/uio.h>

void vmm_nat_init(void);
int transmit_packet(struct iovec *iov, int iovcnt);
int receive_packet(struct iovec *iov, int iovcnt);
