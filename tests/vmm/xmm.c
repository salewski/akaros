#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <parlib/parlib.h>
#include <vmm/vmm.h>

#define ITER 100000

struct virtual_machine local_vm, *vm = &local_vm;
struct vmm_gpcore_init gpci;

unsigned long long *p512, *p1, *p2m;
unsigned long long stack[512];

static volatile int guestcount, hostcount;
static volatile int fucked;
static uint8_t guest[16];
static char *_ = "deadbeefbeefdead";

void hexdump(FILE *f, void *v, int length);
void store(uint8_t *x)
{
	__asm__ __volatile__ ("movdqu %%xmm0, %[x]\n": [x] "=m" (*x));
}
void load(uint8_t *x)
{
	__asm__ __volatile__ ("movdqu %[x], %%xmm0\n":: [x] "m" (*x));
}
void vmexit()
{
		/*
		 * It seems to be hard to turn off printing sometimes.
		 * Just have it print a null. This also makes it easy to
		 * track vmcalls with strace (each vmcall is a write to 1).
		 */
		__asm__ __volatile__("xorw %di, %di\n\tvmcall\n\t");

}
static void vmcall(void *a)
{
	load((uint8_t*)_);
	while ((guestcount < ITER) && (! fucked)) {
		store(guest);
		/* hand code memcmp */
		for(int i = 0; i < 16; i++)
			if (_[i] != guest[i])
				fucked++;
			if (fucked)
				store(guest);
		vmexit();
		guestcount++;
	}
	while(1);
}


void *page(void *addr)
{
	void *v;
	unsigned long flags = MAP_POPULATE | MAP_ANONYMOUS;
	if (addr)
		flags |= MAP_FIXED;

	v = (void *)mmap(addr, 4096, PROT_READ | PROT_WRITE, flags, -1, 0);
	return v;
}

int main(int argc, char **argv)
{
	int vmmflags = 0; // Disabled probably forever. VMM_VMCALL_PRINTF;
	uint64_t entry = (uint64_t)vmcall, kerneladdress = (uint64_t)vmcall;
	struct vm_trapframe *vm_tf;

	parlib_wants_to_be_mcp = FALSE;

	/* sanity */
	load((uint8_t*)_);
	store(guest);
	if (memcmp(_, guest, 16)){
		printf("simple test failed: input:");
		hexdump(stdout, (void*)_, 16);
		printf(":output:");
		hexdump(stdout, guest, 16);printf(":\n");
		exit(1);
	}
	printf("sanity test passed, output:");
	hexdump(stdout, guest, 16);
	printf(":\n");
	memset(guest, 0, 16);

	gpci.posted_irq_desc = page(NULL);
	gpci.vapic_addr = page(NULL);
	gpci.apic_addr = page((void*)0xfee00000);

	vm->nr_gpcs = 1;
	vm->gpcis = &gpci;
	vmm_init(vm, vmmflags);

	p512 = page((void *)0x1000000);
	p1 = page(p512+512);
	p2m = page(p1+512);
	/* Allocate 3 pages for page table pages: a page of 512 GiB
	 * PTEs with only one entry filled to point to a page of 1 GiB
	 * PTEs; a page of 1 GiB PTEs with only one entry filled to
	 * point to a page of 2 MiB PTEs; and a page of 2 MiB PTEs,
	 * only a subset of which will be filled. */

	/* Set up a 1:1 ("identity") page mapping from guest virtual
	 * to guest physical using the (host virtual)
	 * `kerneladdress`. This mapping is used for only a short
	 * time, until the guest sets up its own page tables. Be aware
	 * that the values stored in the table are physical addresses.
	 * This is subtle and mistakes are easily disguised due to the
	 * identity mapping, so take care when manipulating these
	 * mappings. */

	p512[PML4(kerneladdress)] = (uint64_t)p1 | PTE_KERN_RW;
	p1[PML3(kerneladdress)] = (uint64_t)p2m | PTE_KERN_RW;
	p1[PML3(kerneladdress)] = PTE_PS | PTE_KERN_RW;
	for (uintptr_t i = 0; i < 1024*1024*1024; i += PML2_PTE_REACH) {
		p2m[PML2(kerneladdress + i)] =
		    (uint64_t)(kerneladdress + i) | PTE_KERN_RW | PTE_PS;
	}

	vm_tf = gth_to_vmtf(vm->gths[0]);
	vm_tf->tf_cr3 = (uint64_t) p512;
	vm_tf->tf_rip = entry /*+ 4;*/;
	vm_tf->tf_rsp = (uint64_t) (stack+511);
	vm_tf->tf_rdi = (uint64_t) guestcount;
	start_guest_thread(vm->gths[0]);
	uthread_usleep(1000000);

	char *_ = "0xaaffeeffeeaabbcc";
	uint8_t data[16];

	load((uint8_t*)_);
	store(data);
	printf("initial host state: "); hexdump(stdout, data, 16); printf("\n");
	load((uint8_t*)_);

	while ((guestcount < ITER) && (! fucked)) {
		hostcount++;
		if (0 && guestcount % 1000 == 0)
			printf("%d guest iterations\n", guestcount);
		if (0 && hostcount % 1000 == 0) {
			printf("%d host iterations\n", hostcount);
			sleep(1);
		}
		usleep(1);
		/* need to reload after usleep - FP isn't saved across a syscall (which
		 * is a SW ctx). */
		load((uint8_t*)_);
	}
	store(data);
	if (fucked) {
		printf("we're fucked after %d guest iterations, %d host iterations\n",
		       guestcount, hostcount);
	} else {
		printf("We're fine\n");
	}
	printf("guest says: "); hexdump(stdout, guest, 16); printf("\n");
	printf("host says: "); hexdump(stdout, data, 16); printf("\n");

	return 0;
}
