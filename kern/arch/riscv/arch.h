#ifndef ROS_INC_ARCH_H
#define ROS_INC_ARCH_H

#include <ros/arch/arch.h>
#include <arch/mmu.h>
#include <ros/common.h>
#include <ros/arch/membar.h>
#include <arch/riscv.h>
#include <arch/trap.h>
#include <arch/time.h>

/* Arch Constants */
#define HW_CACHE_ALIGN 64

#ifdef __riscv64
# define KERN64
#endif

void print_cpuinfo(void);
void show_mapping(uintptr_t start, size_t size);
void backtrace(void);

static __inline void
breakpoint(void)
{
	asm volatile ("break");
}

static __inline void
tlbflush(void)
{
	lcr3(rcr3());
}

static __inline void 
invlpg(void *addr)
{ 
	tlbflush();
}

static __inline void
icache_flush_page(void* va, void* kva)
{
	asm volatile ("fence.i");
}

static __inline uint64_t
read_tsc(void)
{
	unsigned long t;
	asm volatile ("rdtime %0" : "=r"(t));
	return t;
}

/* Continuing the poor tradition of x86 opcode functions... */
static __inline uint64_t
read_tscp(void)
{
	return read_tsc();
}

static __inline uint64_t 
read_tsc_serialized(void)
{
	mb();
	return read_tsc();
}

static __inline uintptr_t
enable_irq(void)
{
	return setpcr(PCR_SR, SR_ET);
}

static __inline uintptr_t
disable_irq(void)
{
	return clearpcr(PCR_SR, SR_ET);
}

static __inline void
restore_irq(uintptr_t val)
{
	mtpcr(PCR_SR, val);
}

static __inline int
irq_is_enabled(void)
{
	return mfpcr(PCR_SR) & SR_ET;
}

static __inline void
enable_irqsave(int8_t* state)
{
	// *state tracks the number of nested enables and disables
	// initial value of state: 0 = first run / no favorite
	// > 0 means more enabled calls have been made
	// < 0 means more disabled calls have been made
	// Mostly doing this so we can call disable_irqsave first if we want

	// one side or another "gets a point" if interrupts were already the
	// way it wanted to go.  o/w, state stays at 0.  if the state was not 0
	// then, enabling/disabling isn't even an option.  just increment/decrement

	// if enabling is winning or tied, make sure it's enabled
	if ((*state == 0) && !irq_is_enabled())
		enable_irq();
	else
		(*state)++;
}

static __inline void
disable_irqsave(int8_t* state)
{
	if ((*state == 0) && irq_is_enabled())
		disable_irq();
	else 
		(*state)--;
}

static __inline void
cpu_relax(void)
{
  for(int i = 0; i < 100; i++)
	  asm ("nop");
}

static __inline void
clflush(uintptr_t* addr)
{
}

/* os_coreid -> hw_coreid */
static __inline int
get_hw_coreid(int coreid)
{
  return coreid;
}

static __inline int
hw_core_id(void)
{
  return mfpcr(PCR_COREID);
}

/* hw_coreid -> os_coreid */
static __inline int
get_os_coreid(int hw_coreid)
{
	return hw_coreid;
}

/* core_id() returns the OS core number, not to be confused with the
 * hardware-specific core identifier (such as the lapic id) returned by
 * hw_core_id() */
static __inline int
core_id(void)
{
	return get_os_coreid(hw_core_id());
}

static __inline void
cache_flush(void)
{
}

static __inline void
reboot(void)
{
	extern void cputchar(int ch);
	cputchar(0);
}

extern void cpu_halt(void);

#endif /* !ROS_INC_ARCH_H */
