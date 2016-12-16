/* Copyright (c) 2016 Google Inc
 * Barret Rhoden <brho@cs.berkeley.edu>
 * See LICENSE for details.
 *
 * Network address translation for VM guests.
 *
 * TODO:
 *
 * - We're working with the IOVs that the guest gave us through virtio.  If we
 *   care, that whole thing is susceptible to time-of-check attacks.  The guest
 *   could be modifying the IOV that we're working on, so we need to not care
 *   too much about that.
 * - Consider having the kernel proto bypass overwrite the src address.  We
 *   don't care about the proto payload.  We might care about IP headers.
 * - Have an option for controlling whether or not we snoop on I/O.
 */

#include <vmm/nat.h>
#include <parlib/iovec.h>
#include <iplib/iplib.h>
#include <parlib/ros_debug.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

uint8_t host_v4_addr[IPv4addrlen];
uint8_t guest_v4_addr[IPv4addrlen];
int snoop_fd;

/* We map between host port and guest port for a given protcol.  We don't care
 * about confirming IP addresses or anything - we just swap ports. */
struct ip_nat_map {
	uint16_t					guest_port;
	uint16_t					host_port;
	uint8_t						protocol;
	int							host_data_fd;
	// XXX timeout info
};

static void hexdump_packet(struct iovec *iov, int iovcnt);

static void snoop_on_virtio(void)
{
	int ret;
	int srvfd;
	int pipefd[2];
	char buf[MAX_PATH_LEN];

	ret = pipe(pipefd);
	assert(!ret);
	snoop_fd = pipefd[0];
	ret = snprintf(buf, sizeof(buf), "#srv/%s-%d", "snoop", getpid());
	srvfd = open(buf, O_RDWR | O_EXCL | O_CREAT | O_REMCLO, 0444);
	assert(srvfd >= 0);
	ret = snprintf(buf, sizeof(buf), "%d", pipefd[1]);
	ret = write(srvfd, buf, ret);
}

void vmm_nat_init(void)
{
	snoop_on_virtio();
}

static int handle_eth_tx(struct iovec *iov, int iovcnt)
{
	hexdump_packet(iov, iovcnt);
	return 0;
}

static int handle_ipv4_tx(struct iovec *iov, int iovcnt)
{
	uint16_t guest_src_port;
	uint8_t version, protocol;
	size_t proto_hdr_start;

	if (!iov_has_bytes(iov, iovcnt, IP_VER4)) {
		fprintf(stderr, "Short IPv4 header, dropping!\n");
		return -1;
	}
	protocol = iov_get_byte(iov, iovcnt, 9);
	proto_hdr_start = iov_get_byte(iov, iovcnt, 0) & 0x0f;


	// XXX don't forget if it is for the router/host, to remap to us.
	//
	// will need to handle things like DHCP:  (same issues as ARP)
	//
//        ether(s=1c:b7:2c:ee:52:69 d=ff:ff:ff:ff:ff:ff pr=0800 ln=342)
//        ip(s=0.0.0.0 d=255.255.255.255 id=0000 frag=0000 ttl= 64 pr=17 ln=328 hl=20)
//        udp(s=68 d=67 ck=52ab ln= 308)
//        bootp(t=Req ht=1 hl=6 hp=0 xid=e10c5958 sec=29 fl=0000 ca=0.0.0.0 ya=0.0.0.0 sa=0.0.0.0 ga=0.0.0.0 cha=1c:b7:2c:ee:52:69 magic=63825363)
//        dhcp(t=Discover clientid=011cb72cee5269 maxmsg=576  requested=(1 3 6 12 15 28 42) vendorclass=udhcp 1.24.2)
//
}

static int handle_ipv6_tx(struct iovec *iov, int iovcnt)
{
	return -1;
}

int transmit_packet(struct iovec *iov, int iovcnt)
{
	uint16_t ether_type;
	int ret;

	writev(snoop_fd, iov, iovcnt);
	if (!iov_has_bytes(iov, iovcnt, ETH_HDR_LEN)) {
		fprintf(stderr, "Short ethernet frame from the guest, dropping!\n");
		return -1;
	}
	ether_type = iov_get_be16(iov, iovcnt, 12);
	iov_strip_bytes(iov, iovcnt, ETH_HDR_LEN);

	switch (ether_type) {
	case ETH_TYPE_ARP:
		// XXX crap - we actually want to send them a response now.
		// maybe kick the rx uthread
		ret = handle_eth_tx(iov, iovcnt);
		break;
	case ETH_TYPE_IPV4:
		ret = handle_ipv4_tx(iov, iovcnt);
		break;
	case ETH_TYPE_IPV6:
		ret = handle_ipv6_tx(iov, iovcnt);
		break;
	default:
		fprintf(stderr, "Unknown ethertype 0x%x, dropping!\n", ether_type);
		return -1;
	};
	return ret;
}

// XXX somewhere else, we need someone reading on all of the open convs
// 		could be the one virtio thread, or we could have N threads
// 		might be easier to tap them all
// 		o/w the virtio thread will need to block on a chan that all the others
// 		can push down
// 			linked list of iov[] arrays

// XXX this is the guest receiving a packet.  the virtio thread is usually
// blocked in here, i think
int receive_packet(struct iovec *iov, int iovcnt)
{

	// XXX i think they want to block
	sleep(9999999999);
	/* once we know what we want, we can write it out */
	writev(snoop_fd, iov, iovcnt);

}

static void hexdump_packet(struct iovec *iov, int iovcnt)
{
	uint8_t *buf;
	size_t len;

	len = iov_get_len(iov, iovcnt);
	buf = malloc(len);
	iov_linearize(iov, iovcnt, buf, len);
	fprintf(stderr, "Dumping packet:\n------------\n");
	hexdump(stderr, buf, len);
	fflush(stderr);
	free(buf);
}
