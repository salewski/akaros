/* Copyright (c) 2016 Google Inc
 * Barret Rhoden <brho@cs.berkeley.edu>
 * See LICENSE for details.
 *
 * Various iovec utility functions. */

#pragma once

#include <sys/uio.h>

void iov_strip_bytes(struct iovec *iov, int iovcnt, size_t amt);
