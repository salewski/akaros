/* Copyright (c) 2016 Google Inc.
 * See LICENSE for details.
 *
 * Linux emulation for virtual machines. */

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <vmm/vmm.h>
#include <errno.h>
#include <sys/syscall.h>
#include <vmm/linux_syscalls.h>
#include <sys/time.h>
#include <vmm/linuxemu.h>
#include <dlfcn.h>
#include <sys/mman.h>
#include <futex.h>

static int lemu_debug;

static uth_mutex_t *lemu_logging_lock;

static FILE *lemu_global_logfile;

void init_lemu_logging(int log_level)
{
	const char *logfile_name = "lemu.log";
	FILE *x = fopen(logfile_name, "w");

	lemu_debug = log_level;
	lemu_logging_lock = uth_mutex_alloc();

	if (x != NULL)
		lemu_global_logfile = x;
	else
		lemu_global_logfile = stderr;
}

void destroy_lemu_logging(void)
{
	if (lemu_logging_lock != NULL)
		uth_mutex_free(lemu_logging_lock);

	if (lemu_global_logfile != stderr)
		fclose(lemu_global_logfile);
}


/////////////////////////////////////
// BEGIN DUNE SYSCALL IMPLEMENTATIONS
/////////////////////////////////////

//////////////////////////////////////////////////////
// Ported Syscalls (we just call the akaros version)
//////////////////////////////////////////////////////

bool dune_sys_ftruncate(struct vm_trapframe *tf)
{
	int retval = ftruncate((int) tf->tf_rdi, (off_t) tf->tf_rsi);
	int err = errno;

	if (retval == -1) {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, true,
		          "ERROR %d\n", err);
		tf->tf_rax = -err;
	} else {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, false,
		          "SUCCESS %d\n", retval);
		tf->tf_rax = retval;
	}
	return true;

}

bool dune_sys_fcntl(struct vm_trapframe *tf)
{
	int retval = fcntl((int) tf->tf_rdi, (int) tf->tf_rsi, (int) tf->tf_rdx);
	int err = errno;

	if (retval == -1) {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, true,
		          "ERROR %d\n", err);
		tf->tf_rax = -err;
	} else {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, false,
		          "SUCCESS %d\n", retval);
		tf->tf_rax = retval;
	}
	return true;
}

bool dune_sys_pread64(struct vm_trapframe *tf)
{
	int fd = (int) tf->tf_rdi;
	void *buf = (void*) tf->tf_rsi;
	size_t count = tf->tf_rdx;
	off_t offset = (off_t) tf->tf_r10;

	ssize_t retval = pread(fd, buf, count, offset);
	int err = errno;

	if (retval == -1) {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, true,
		          "ERROR %zd\n", err);
		tf->tf_rax = -err;
	} else {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, false,
		          "SUCCESS %zd\n", retval);
		tf->tf_rax = retval;
	}
	return true;
}

bool dune_sys_read(struct vm_trapframe *tf)
{
	ssize_t retval = read(tf->tf_rdi, (void*) tf->tf_rsi, (size_t) tf->tf_rdx);
	int err = errno;

	if (retval == -1) {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, true,
		          "ERROR %zd\n", err);
		tf->tf_rax = -err;
	} else {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, false,
		          "SUCCESS %zd\n", retval);
		tf->tf_rax = retval;
	}
	return true;
}

bool dune_sys_pwrite64(struct vm_trapframe *tf)
{
	int fd = (int) tf->tf_rdi;
	const void *buf = (const void*) tf->tf_rsi;
	size_t length = (size_t) tf->tf_rdx;
	off_t offset = (off_t) tf->tf_r10;

	ssize_t retval = pwrite(fd, buf, length, offset);
	int err = errno;

	if (retval == -1) {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, true,
		          "ERROR %zd\n", err);
		tf->tf_rax = -err;
	} else {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, false,
		          "SUCCESS %zd\n", retval);
		tf->tf_rax = retval;
	}
	return true;
}

bool dune_sys_write(struct vm_trapframe *tf)
{
	ssize_t retval = write((int) tf->tf_rdi, (const void *) tf->tf_rsi,
	                       (size_t) tf->tf_rdx);
	int err = errno;

	if (retval == -1) {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, true,
		          "ERROR %zd\n", err);
		tf->tf_rax = -err;
	} else {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, false,
		          "SUCCESS %zd\n", retval);
		tf->tf_rax = retval;
	}
	return true;
}

bool dune_sys_getpid(struct vm_trapframe *tf)
{
	// Getpid always suceeds
	int retval = getpid();

	lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, false, "SUCCESS %d\n", retval);
	tf->tf_rax = retval;
	return true;
}

bool dune_sys_gettimeofday(struct vm_trapframe *tf)
{
	int retval = gettimeofday((struct timeval*) tf->tf_rdi,
	                          (struct timezone*) tf->tf_rsi);
	int err = errno;

	if (retval == -1) {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, true,
		          "ERROR %d\n", err);
		tf->tf_rax = -err;
	} else {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, false,
		          "SUCCESS %d\n", retval);
		tf->tf_rax = retval;
	}
	return true;
}

bool dune_sys_dup(struct vm_trapframe *tf)
{
	int retval = dup((int) tf->tf_rdi);
	int err = errno;

	if (retval == -1) {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, true,
		          "ERROR %d\n", err);
		tf->tf_rax = -err;
	} else {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, false,
		          "SUCCESS %d\n", retval);
		tf->tf_rax = retval;
	}
	return true;
}

bool dune_sys_dup2(struct vm_trapframe *tf)
{
	int retval = dup2((int) tf->tf_rdi, (int)tf->tf_rsi);
	int err = errno;

	if (retval == -1) {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, true,
		          "ERROR %d\n", err);
		tf->tf_rax = -err;
	} else {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, false,
		          "SUCCESS %d\n", retval);
		tf->tf_rax = retval;
	}
	return true;
}

bool dune_sys_umask(struct vm_trapframe *tf)
{
	//Umask always succeeds
	int retval = umask((mode_t) tf->tf_rdi);

	lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, false, "SUCCESS %d\n", retval);
	tf->tf_rax = retval;
	return true;
}

bool dune_sys_clock_gettime(struct vm_trapframe *tf)
{
	int retval = clock_gettime(tf->tf_rdi, (struct timespec*) tf->tf_rsi);
	int err = errno;

	if (retval == -1) {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, true,
		          "ERROR %d\n", err);
		tf->tf_rax = -err;
	} else {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, false,
		          "SUCCESS %d\n", retval);
		tf->tf_rax = retval;
	}
	return true;
}

bool dune_sys_munmap(struct vm_trapframe *tf)
{
	int retval = munmap((void *) tf->tf_rdi, tf->tf_rsi);
	int err = errno;

	if (retval == -1) {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, true,
		          "ERROR %d\n", err);
		tf->tf_rax = -err;
	} else {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, false,
		          "SUCCESS %d\n", retval);
		tf->tf_rax = retval;
	}
	return true;
}

bool dune_sys_futex(struct vm_trapframe *tf)
{
	int *uaddr = (int*) tf->tf_rdi;
	int op = (int) tf->tf_rsi;
	int val = (int) tf->tf_rdx;
	struct timespec *timeout = (struct timespec *) tf->tf_r10;
	int *uaddr2 = (int*) tf->tf_r8;
	int val3 = (int) tf->tf_r9;
	int retval = futex(uaddr, op, val, timeout, uaddr2, val3);
	int err = errno;

	if (retval == -1) {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, true,
		          "ERROR %d\n", err);
		tf->tf_rax = -err;
	} else {
		lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, false,
		          "SUCCESS %d\n", retval);
		tf->tf_rax = retval;
	}
	return true;
}

bool dune_sys_gettid(struct vm_trapframe *tf)
{
	// Gettid always succeeds
	int retval = tf->tf_guest_pcoreid;

	lemuprint(tf->tf_guest_pcoreid, tf->tf_rax, false, "SUCCESS %d\n", retval);
	tf->tf_rax = retval;
	return true;
}

/////////////////////////////////////////////////////////
// Modified Syscalls (Partially implemented in Akaros)
////////////////////////////////////////////////////////

bool dune_sys_open(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

bool dune_sys_openat(struct vm_trapframe *tf)
{

	// To Be Implemented
	return false;
}

bool dune_sys_readlinkat(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

bool dune_sys_unlinkat(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

bool dune_sys_close(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

bool dune_sys_sched_yield(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}


bool dune_sys_fstat(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

bool dune_sys_stat(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

///////////////////////////////////////////////////
// Newly Implemented Syscalls
///////////////////////////////////////////////////

// Fallocate syscall
bool dune_sys_fallocate(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

bool dune_sys_sched_getaffinity(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

bool dune_sys_pselect6(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

bool dune_sys_getrandom(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

/////////////////////////////////////////////////////////////////
/// We don't have a good implementation for these syscalls,
/// they will not work in all cases
/////////////////////////////////////////////////////////////////

bool dune_sys_getgroups(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}


bool dune_sys_geteuid(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

bool dune_sys_getegid(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}


bool dune_sys_getuid(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

bool dune_sys_getgid(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

bool dune_sys_mincore(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

bool dune_sys_rt_sigprocmask(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

bool dune_sys_sigaltstack(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

bool dune_sys_rt_sigaction(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

bool dune_sys_epoll_create1(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

bool dune_sys_epoll_wait(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

// Unimplemented
bool dune_sys_epoll_ctl(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

// Unimplemented
bool dune_sys_fstatfs(struct vm_trapframe *tf)
{
	// To Be Implemented
	return false;
}

// Main syscall table
struct dune_sys_table_entry dune_syscall_table[DUNE_MAX_NUM_SYSCALLS] = {
	[DUNE_SYS_READ] = {dune_sys_read, "DUNE_SYS_READ"},
	[DUNE_SYS_WRITE] = {dune_sys_write, "DUNE_SYS_WRITE"},
	[DUNE_SYS_OPEN] = {dune_sys_open, "DUNE_SYS_OPEN"},
	[DUNE_SYS_CLOSE] = {dune_sys_close, "DUNE_SYS_CLOSE"},
	[DUNE_SYS_STAT] = {dune_sys_stat, "DUNE_SYS_STAT"},
	[DUNE_SYS_FSTAT] = {dune_sys_fstat, "DUNE_SYS_FSTAT"},
	[DUNE_SYS_LSTAT] = {NULL, "DUNE_SYS_LSTAT"},
	[DUNE_SYS_POLL] = {NULL, "DUNE_SYS_POLL"},
	[DUNE_SYS_LSEEK] = {NULL, "DUNE_SYS_LSEEK"},
	[DUNE_SYS_MMAP] = {NULL, "DUNE_SYS_MMAP"},
	[DUNE_SYS_MPROTECT] = {NULL, "DUNE_SYS_MPROTECT"},
	[DUNE_SYS_MUNMAP] = {dune_sys_munmap, "DUNE_SYS_MUNMAP"},
	[DUNE_SYS_BRK] = {NULL, "DUNE_SYS_BRK"},
	[DUNE_SYS_RT_SIGACTION] = {dune_sys_rt_sigaction, "DUNE_SYS_RT_SIGACTION"},
	[DUNE_SYS_RT_SIGPROCMASK] = {dune_sys_rt_sigprocmask,
	                             "DUNE_SYS_RT_SIGPROCMASK"},
	[DUNE_SYS_RT_SIGRETURN] = {NULL, "DUNE_SYS_RT_SIGRETURN"},
	[DUNE_SYS_IOCTL] = {NULL, "DUNE_SYS_IOCTL"},
	[DUNE_SYS_PREAD64] = {dune_sys_pread64, "DUNE_SYS_PREAD64"},
	[DUNE_SYS_PWRITE64] = {dune_sys_pwrite64, "DUNE_SYS_PWRITE64"},
	[DUNE_SYS_READV] = {NULL, "DUNE_SYS_READV"},
	[DUNE_SYS_WRITEV] = {NULL, "DUNE_SYS_WRITEV"},
	[DUNE_SYS_ACCESS] = {NULL, "DUNE_SYS_ACCESS"},
	[DUNE_SYS_PIPE] = {NULL, "DUNE_SYS_PIPE"},
	[DUNE_SYS_SELECT] = {NULL, "DUNE_SYS_SELECT"},
	[DUNE_SYS_SCHED_YIELD] = {dune_sys_sched_yield, "DUNE_SYS_SCHED_YIELD"},
	[DUNE_SYS_MREMAP] = {NULL, "DUNE_SYS_MREMAP"},
	[DUNE_SYS_MSYNC] = {NULL, "DUNE_SYS_MSYNC"},
	[DUNE_SYS_MINCORE] = {dune_sys_mincore, "DUNE_SYS_MINCORE"},
	[DUNE_SYS_MADVISE] = {NULL, "DUNE_SYS_MADVISE"},
	[DUNE_SYS_SHMGET] = {NULL, "DUNE_SYS_SHMGET"},
	[DUNE_SYS_SHMAT] = {NULL, "DUNE_SYS_SHMAT"},
	[DUNE_SYS_SHMCTL] = {NULL, "DUNE_SYS_SHMCTL"},
	[DUNE_SYS_DUP] = {dune_sys_dup, "DUNE_SYS_DUP"},
	[DUNE_SYS_DUP2] = {dune_sys_dup2, "DUNE_SYS_DUP2"},
	[DUNE_SYS_PAUSE] = {NULL, "DUNE_SYS_PAUSE"},
	[DUNE_SYS_NANOSLEEP] = {NULL, "DUNE_SYS_NANOSLEEP"},
	[DUNE_SYS_GETITIMER] = {NULL, "DUNE_SYS_GETITIMER"},
	[DUNE_SYS_ALARM] = {NULL, "DUNE_SYS_ALARM"},
	[DUNE_SYS_SETITIMER] = {NULL, "DUNE_SYS_SETITIMER"},
	[DUNE_SYS_GETPID] = {dune_sys_getpid, "DUNE_SYS_GETPID"},
	[DUNE_SYS_SENDFILE] = {NULL, "DUNE_SYS_SENDFILE"},
	[DUNE_SYS_SOCKET] = {NULL, "DUNE_SYS_SOCKET"},
	[DUNE_SYS_CONNECT] = {NULL, "DUNE_SYS_CONNECT"},
	[DUNE_SYS_ACCEPT] = {NULL, "DUNE_SYS_ACCEPT"},
	[DUNE_SYS_SENDTO] = {NULL, "DUNE_SYS_SENDTO"},
	[DUNE_SYS_RECVFROM] = {NULL, "DUNE_SYS_RECVFROM"},
	[DUNE_SYS_SENDMSG] = {NULL, "DUNE_SYS_SENDMSG"},
	[DUNE_SYS_RECVMSG] = {NULL, "DUNE_SYS_RECVMSG"},
	[DUNE_SYS_SHUTDOWN] = {NULL, "DUNE_SYS_SHUTDOWN"},
	[DUNE_SYS_BIND] = {NULL, "DUNE_SYS_BIND"},
	[DUNE_SYS_LISTEN] = {NULL, "DUNE_SYS_LISTEN"},
	[DUNE_SYS_GETSOCKNAME] = {NULL, "DUNE_SYS_GETSOCKNAME"},
	[DUNE_SYS_GETPEERNAME] = {NULL, "DUNE_SYS_GETPEERNAME"},
	[DUNE_SYS_SOCKETPAIR] = {NULL, "DUNE_SYS_SOCKETPAIR"},
	[DUNE_SYS_SETSOCKOPT] = {NULL, "DUNE_SYS_SETSOCKOPT"},
	[DUNE_SYS_GETSOCKOPT] = {NULL, "DUNE_SYS_GETSOCKOPT"},
	[DUNE_SYS_CLONE] = {NULL, "DUNE_SYS_CLONE"},
	[DUNE_SYS_FORK] = {NULL, "DUNE_SYS_FORK"},
	[DUNE_SYS_VFORK] = {NULL, "DUNE_SYS_VFORK"},
	[DUNE_SYS_EXECVE] = {NULL, "DUNE_SYS_EXECVE"},
	[DUNE_SYS_EXIT] = {NULL, "DUNE_SYS_EXIT"},
	[DUNE_SYS_WAIT4] = {NULL, "DUNE_SYS_WAIT4"},
	[DUNE_SYS_KILL] = {NULL, "DUNE_SYS_KILL"},
	[DUNE_SYS_UNAME] = {NULL, "DUNE_SYS_UNAME"},
	[DUNE_SYS_SEMGET] = {NULL, "DUNE_SYS_SEMGET"},
	[DUNE_SYS_SEMOP] = {NULL, "DUNE_SYS_SEMOP"},
	[DUNE_SYS_SEMCTL] = {NULL, "DUNE_SYS_SEMCTL"},
	[DUNE_SYS_SHMDT] = {NULL, "DUNE_SYS_SHMDT"},
	[DUNE_SYS_MSGGET] = {NULL, "DUNE_SYS_MSGGET"},
	[DUNE_SYS_MSGSND] = {NULL, "DUNE_SYS_MSGSND"},
	[DUNE_SYS_MSGRCV] = {NULL, "DUNE_SYS_MSGRCV"},
	[DUNE_SYS_MSGCTL] = {NULL, "DUNE_SYS_MSGCTL"},
	[DUNE_SYS_FCNTL] = {dune_sys_fcntl, "DUNE_SYS_FCNTL"},
	[DUNE_SYS_FLOCK] = {NULL, "DUNE_SYS_FLOCK"},
	[DUNE_SYS_FSYNC] = {NULL, "DUNE_SYS_FSYNC"},
	[DUNE_SYS_FDATASYNC] = {NULL, "DUNE_SYS_FDATASYNC"},
	[DUNE_SYS_TRUNCATE] = {NULL, "DUNE_SYS_TRUNCATE"},
	[DUNE_SYS_FTRUNCATE] = {dune_sys_ftruncate, "DUNE_SYS_FTRUNCATE"},
	[DUNE_SYS_GETDENTS] = {NULL, "DUNE_SYS_GETDENTS"},
	[DUNE_SYS_GETCWD] = {NULL, "DUNE_SYS_GETCWD"},
	[DUNE_SYS_CHDIR] = {NULL, "DUNE_SYS_CHDIR"},
	[DUNE_SYS_FCHDIR] = {NULL, "DUNE_SYS_FCHDIR"},
	[DUNE_SYS_RENAME] = {NULL, "DUNE_SYS_RENAME"},
	[DUNE_SYS_MKDIR] = {NULL, "DUNE_SYS_MKDIR"},
	[DUNE_SYS_RMDIR] = {NULL, "DUNE_SYS_RMDIR"},
	[DUNE_SYS_CREAT] = {NULL, "DUNE_SYS_CREAT"},
	[DUNE_SYS_LINK] = {NULL, "DUNE_SYS_LINK"},
	[DUNE_SYS_UNLINK] = {NULL, "DUNE_SYS_UNLINK"},
	[DUNE_SYS_SYMLINK] = {NULL, "DUNE_SYS_SYMLINK"},
	[DUNE_SYS_READLINK] = {NULL, "DUNE_SYS_READLINK"},
	[DUNE_SYS_CHMOD] = {NULL, "DUNE_SYS_CHMOD"},
	[DUNE_SYS_FCHMOD] = {NULL, "DUNE_SYS_FCHMOD"},
	[DUNE_SYS_CHOWN] = {NULL, "DUNE_SYS_CHOWN"},
	[DUNE_SYS_FCHOWN] = {NULL, "DUNE_SYS_FCHOWN"},
	[DUNE_SYS_LCHOWN] = {NULL, "DUNE_SYS_LCHOWN"},
	[DUNE_SYS_UMASK] = {dune_sys_umask, "DUNE_SYS_UMASK"},
	[DUNE_SYS_GETTIMEOFDAY] = {dune_sys_gettimeofday, "DUNE_SYS_GETTIMEOFDAY"},
	[DUNE_SYS_GETRLIMIT] = {NULL, "DUNE_SYS_GETRLIMIT"},
	[DUNE_SYS_GETRUSAGE] = {NULL, "DUNE_SYS_GETRUSAGE"},
	[DUNE_SYS_SYSINFO] = {NULL, "DUNE_SYS_SYSINFO"},
	[DUNE_SYS_TIMES] = {NULL, "DUNE_SYS_TIMES"},
	[DUNE_SYS_PTRACE] = {NULL, "DUNE_SYS_PTRACE"},
	[DUNE_SYS_GETUID] = {dune_sys_getuid, "DUNE_SYS_GETUID"},
	[DUNE_SYS_SYSLOG] = {NULL, "DUNE_SYS_SYSLOG"},
	[DUNE_SYS_GETGID] = {dune_sys_getgid, "DUNE_SYS_GETGID"},
	[DUNE_SYS_SETUID] = {NULL, "DUNE_SYS_SETUID"},
	[DUNE_SYS_SETGID] = {NULL, "DUNE_SYS_SETGID"},
	[DUNE_SYS_GETEUID] = {dune_sys_geteuid, "DUNE_SYS_GETEUID"},
	[DUNE_SYS_GETEGID] = {dune_sys_getegid, "DUNE_SYS_GETEGID"},
	[DUNE_SYS_SETPGID] = {NULL, "DUNE_SYS_SETPGID"},
	[DUNE_SYS_GETPPID] = {NULL, "DUNE_SYS_GETPPID"},
	[DUNE_SYS_GETPGRP] = {NULL, "DUNE_SYS_GETPGRP"},
	[DUNE_SYS_SETSID] = {NULL, "DUNE_SYS_SETSID"},
	[DUNE_SYS_SETREUID] = {NULL, "DUNE_SYS_SETREUID"},
	[DUNE_SYS_SETREGID] = {NULL, "DUNE_SYS_SETREGID"},
	[DUNE_SYS_GETGROUPS] = {dune_sys_getgroups, "DUNE_SYS_GETGROUPS"},
	[DUNE_SYS_SETGROUPS] = {NULL, "DUNE_SYS_SETGROUPS"},
	[DUNE_SYS_SETRESUID] = {NULL, "DUNE_SYS_SETRESUID"},
	[DUNE_SYS_GETRESUID] = {NULL, "DUNE_SYS_GETRESUID"},
	[DUNE_SYS_SETRESGID] = {NULL, "DUNE_SYS_SETRESGID"},
	[DUNE_SYS_GETRESGID] = {NULL, "DUNE_SYS_GETRESGID"},
	[DUNE_SYS_GETPGID] = {NULL, "DUNE_SYS_GETPGID"},
	[DUNE_SYS_SETFSUID] = {NULL, "DUNE_SYS_SETFSUID"},
	[DUNE_SYS_SETFSGID] = {NULL, "DUNE_SYS_SETFSGID"},
	[DUNE_SYS_GETSID] = {NULL, "DUNE_SYS_GETSID"},
	[DUNE_SYS_CAPGET] = {NULL, "DUNE_SYS_CAPGET"},
	[DUNE_SYS_CAPSET] = {NULL, "DUNE_SYS_CAPSET"},
	[DUNE_SYS_RT_SIGPENDING] = {NULL, "DUNE_SYS_RT_SIGPENDING"},
	[DUNE_SYS_RT_SIGTIMEDWAIT] = {NULL, "DUNE_SYS_RT_SIGTIMEDWAIT"},
	[DUNE_SYS_RT_SIGQUEUEINFO] = {NULL, "DUNE_SYS_RT_SIGQUEUEINFO"},
	[DUNE_SYS_RT_SIGSUSPEND] = {NULL, "DUNE_SYS_RT_SIGSUSPEND"},
	[DUNE_SYS_SIGALTSTACK] = {dune_sys_sigaltstack, "DUNE_SYS_SIGALTSTACK"},
	[DUNE_SYS_UTIME] = {NULL, "DUNE_SYS_UTIME"},
	[DUNE_SYS_MKNOD] = {NULL, "DUNE_SYS_MKNOD"},
	[DUNE_SYS_USELIB] = {NULL, "DUNE_SYS_USELIB"},
	[DUNE_SYS_PERSONALITY] = {NULL, "DUNE_SYS_PERSONALITY"},
	[DUNE_SYS_USTAT] = {NULL, "DUNE_SYS_USTAT"},
	[DUNE_SYS_STATFS] = {NULL, "DUNE_SYS_STATFS"},
	[DUNE_SYS_FSTATFS] = {dune_sys_fstatfs, "DUNE_SYS_FSTATFS"},
	[DUNE_SYS_SYSFS] = {NULL, "DUNE_SYS_SYSFS"},
	[DUNE_SYS_GETPRIORITY] = {NULL, "DUNE_SYS_GETPRIORITY"},
	[DUNE_SYS_SETPRIORITY] = {NULL, "DUNE_SYS_SETPRIORITY"},
	[DUNE_SYS_SCHED_SETPARAM] = {NULL, "DUNE_SYS_SCHED_SETPARAM"},
	[DUNE_SYS_SCHED_GETPARAM] = {NULL, "DUNE_SYS_SCHED_GETPARAM"},
	[DUNE_SYS_SCHED_SETSCHEDULER] = {NULL, "DUNE_SYS_SCHED_SETSCHEDULER"},
	[DUNE_SYS_SCHED_GETSCHEDULER] = {NULL, "DUNE_SYS_SCHED_GETSCHEDULER"},
	[DUNE_SYS_SCHED_GET_PRIORITY_MAX] = {NULL,
	                                    "DUNE_SYS_SCHED_GET_PRIORITY_MAX"},
	[DUNE_SYS_SCHED_GET_PRIORITY_MIN] = {NULL,
	                                    "DUNE_SYS_SCHED_GET_PRIORITY_MIN"},
	[DUNE_SYS_SCHED_RR_GET_INTERVAL] = {NULL, "DUNE_SYS_SCHED_RR_GET_INTERVAL"},
	[DUNE_SYS_MLOCK] = {NULL, "DUNE_SYS_MLOCK"},
	[DUNE_SYS_MUNLOCK] = {NULL, "DUNE_SYS_MUNLOCK"},
	[DUNE_SYS_MLOCKALL] = {NULL, "DUNE_SYS_MLOCKALL"},
	[DUNE_SYS_MUNLOCKALL] = {NULL, "DUNE_SYS_MUNLOCKALL"},
	[DUNE_SYS_VHANGUP] = {NULL, "DUNE_SYS_VHANGUP"},
	[DUNE_SYS_MODIFY_LDT] = {NULL, "DUNE_SYS_MODIFY_LDT"},
	[DUNE_SYS_PIVOT_ROOT] = {NULL, "DUNE_SYS_PIVOT_ROOT"},
	[DUNE_SYS__SYSCTL] = {NULL, "DUNE_SYS__SYSCTL"},
	[DUNE_SYS_PRCTL] = {NULL, "DUNE_SYS_PRCTL"},
	[DUNE_SYS_ARCH_PRCTL] = {NULL, "DUNE_SYS_ARCH_PRCTL"},
	[DUNE_SYS_ADJTIMEX] = {NULL, "DUNE_SYS_ADJTIMEX"},
	[DUNE_SYS_SETRLIMIT] = {NULL, "DUNE_SYS_SETRLIMIT"},
	[DUNE_SYS_CHROOT] = {NULL, "DUNE_SYS_CHROOT"},
	[DUNE_SYS_SYNC] = {NULL, "DUNE_SYS_SYNC"},
	[DUNE_SYS_ACCT] = {NULL, "DUNE_SYS_ACCT"},
	[DUNE_SYS_SETTIMEOFDAY] = {NULL, "DUNE_SYS_SETTIMEOFDAY"},
	[DUNE_SYS_MOUNT] = {NULL, "DUNE_SYS_MOUNT"},
	[DUNE_SYS_UMOUNT2] = {NULL, "DUNE_SYS_UMOUNT2"},
	[DUNE_SYS_SWAPON] = {NULL, "DUNE_SYS_SWAPON"},
	[DUNE_SYS_SWAPOFF] = {NULL, "DUNE_SYS_SWAPOFF"},
	[DUNE_SYS_REBOOT] = {NULL, "DUNE_SYS_REBOOT"},
	[DUNE_SYS_SETHOSTNAME] = {NULL, "DUNE_SYS_SETHOSTNAME"},
	[DUNE_SYS_SETDOMAINNAME] = {NULL, "DUNE_SYS_SETDOMAINNAME"},
	[DUNE_SYS_IOPL] = {NULL, "DUNE_SYS_IOPL"},
	[DUNE_SYS_IOPERM] = {NULL, "DUNE_SYS_IOPERM"},
	[DUNE_SYS_CREATE_MODULE] = {NULL, "DUNE_SYS_CREATE_MODULE"},
	[DUNE_SYS_INIT_MODULE] = {NULL, "DUNE_SYS_INIT_MODULE"},
	[DUNE_SYS_DELETE_MODULE] = {NULL, "DUNE_SYS_DELETE_MODULE"},
	[DUNE_SYS_GET_KERNEL_SYMS] = {NULL, "DUNE_SYS_GET_KERNEL_SYMS"},
	[DUNE_SYS_QUERY_MODULE] = {NULL, "DUNE_SYS_QUERY_MODULE"},
	[DUNE_SYS_QUOTACTL] = {NULL, "DUNE_SYS_QUOTACTL"},
	[DUNE_SYS_NFSSERVCTL] = {NULL, "DUNE_SYS_NFSSERVCTL"},
	[DUNE_SYS_GETPMSG] = {NULL, "DUNE_SYS_GETPMSG"},
	[DUNE_SYS_PUTPMSG] = {NULL, "DUNE_SYS_PUTPMSG"},
	[DUNE_SYS_AFS_SYSCALL] = {NULL, "DUNE_SYS_AFS_SYSCALL"},
	[DUNE_SYS_TUXCALL] = {NULL, "DUNE_SYS_TUXCALL"},
	[DUNE_SYS_SECURITY] = {NULL, "DUNE_SYS_SECURITY"},
	[DUNE_SYS_GETTID] = {dune_sys_gettid, "DUNE_SYS_GETTID"},
	[DUNE_SYS_READAHEAD] = {NULL, "DUNE_SYS_READAHEAD"},
	[DUNE_SYS_SETXATTR] = {NULL, "DUNE_SYS_SETXATTR"},
	[DUNE_SYS_LSETXATTR] = {NULL, "DUNE_SYS_LSETXATTR"},
	[DUNE_SYS_FSETXATTR] = {NULL, "DUNE_SYS_FSETXATTR"},
	[DUNE_SYS_GETXATTR] = {NULL, "DUNE_SYS_GETXATTR"},
	[DUNE_SYS_LGETXATTR] = {NULL, "DUNE_SYS_LGETXATTR"},
	[DUNE_SYS_FGETXATTR] = {NULL, "DUNE_SYS_FGETXATTR"},
	[DUNE_SYS_LISTXATTR] = {NULL, "DUNE_SYS_LISTXATTR"},
	[DUNE_SYS_LLISTXATTR] = {NULL, "DUNE_SYS_LLISTXATTR"},
	[DUNE_SYS_FLISTXATTR] = {NULL, "DUNE_SYS_FLISTXATTR"},
	[DUNE_SYS_REMOVEXATTR] = {NULL, "DUNE_SYS_REMOVEXATTR"},
	[DUNE_SYS_LREMOVEXATTR] = {NULL, "DUNE_SYS_LREMOVEXATTR"},
	[DUNE_SYS_FREMOVEXATTR] = {NULL, "DUNE_SYS_FREMOVEXATTR"},
	[DUNE_SYS_TKILL] = {NULL, "DUNE_SYS_TKILL"},
	[DUNE_SYS_TIME] = {NULL, "DUNE_SYS_TIME"},
	[DUNE_SYS_FUTEX] = {dune_sys_futex, "DUNE_SYS_FUTEX"},
	[DUNE_SYS_SCHED_SETAFFINITY] = {NULL, "DUNE_SYS_SCHED_SETAFFINITY"},
	[DUNE_SYS_SCHED_GETAFFINITY] = {dune_sys_sched_getaffinity,
	                                "DUNE_SYS_SCHED_GETAFFINITY"},
	[DUNE_SYS_SET_THREAD_AREA] = {NULL, "DUNE_SYS_SET_THREAD_AREA"},
	[DUNE_SYS_IO_SETUP] = {NULL, "DUNE_SYS_IO_SETUP"},
	[DUNE_SYS_IO_DESTROY] = {NULL, "DUNE_SYS_IO_DESTROY"},
	[DUNE_SYS_IO_GETEVENTS] = {NULL, "DUNE_SYS_IO_GETEVENTS"},
	[DUNE_SYS_IO_SUBMIT] = {NULL, "DUNE_SYS_IO_SUBMIT"},
	[DUNE_SYS_IO_CANCEL] = {NULL, "DUNE_SYS_IO_CANCEL"},
	[DUNE_SYS_GET_THREAD_AREA] = {NULL, "DUNE_SYS_GET_THREAD_AREA"},
	[DUNE_SYS_LOOKUP_DCOOKIE] = {NULL, "DUNE_SYS_LOOKUP_DCOOKIE"},
	[DUNE_SYS_EPOLL_CREATE] = {NULL, "DUNE_SYS_EPOLL_CREATE"},
	[DUNE_SYS_EPOLL_CTL_OLD] = {NULL, "DUNE_SYS_EPOLL_CTL_OLD"},
	[DUNE_SYS_EPOLL_WAIT_OLD] = {NULL, "DUNE_SYS_EPOLL_WAIT_OLD"},
	[DUNE_SYS_REMAP_FILE_PAGES] = {NULL, "DUNE_SYS_REMAP_FILE_PAGES"},
	[DUNE_SYS_GETDENTS64] = {NULL, "DUNE_SYS_GETDENTS64"},
	[DUNE_SYS_SET_TID_ADDRESS] = {NULL, "DUNE_SYS_SET_TID_ADDRESS"},
	[DUNE_SYS_RESTART_SYSCALL] = {NULL, "DUNE_SYS_RESTART_SYSCALL"},
	[DUNE_SYS_SEMTIMEDOP] = {NULL, "DUNE_SYS_SEMTIMEDOP"},
	[DUNE_SYS_FADVISE64] = {NULL, "DUNE_SYS_FADVISE64"},
	[DUNE_SYS_TIMER_CREATE] = {NULL, "DUNE_SYS_TIMER_CREATE"},
	[DUNE_SYS_TIMER_SETTIME] = {NULL, "DUNE_SYS_TIMER_SETTIME"},
	[DUNE_SYS_TIMER_GETTIME] = {NULL, "DUNE_SYS_TIMER_GETTIME"},
	[DUNE_SYS_TIMER_GETOVERRUN] = {NULL, "DUNE_SYS_TIMER_GETOVERRUN"},
	[DUNE_SYS_TIMER_DELETE] = {NULL, "DUNE_SYS_TIMER_DELETE"},
	[DUNE_SYS_CLOCK_SETTIME] = {NULL, "DUNE_SYS_CLOCK_SETTIME"},
	[DUNE_SYS_CLOCK_GETTIME] = {dune_sys_clock_gettime,
	                            "DUNE_SYS_CLOCK_GETTIME"},
	[DUNE_SYS_CLOCK_GETRES] = {NULL, "DUNE_SYS_CLOCK_GETRES"},
	[DUNE_SYS_CLOCK_NANOSLEEP] = {NULL, "DUNE_SYS_CLOCK_NANOSLEEP"},
	[DUNE_SYS_EXIT_GROUP] = {NULL, "DUNE_SYS_EXIT_GROUP"},
	[DUNE_SYS_EPOLL_WAIT] = {dune_sys_epoll_wait, "DUNE_SYS_EPOLL_WAIT"},
	[DUNE_SYS_EPOLL_CTL] = {dune_sys_epoll_ctl, "DUNE_SYS_EPOLL_CTL"},
	[DUNE_SYS_TGKILL] = {NULL, "DUNE_SYS_TGKILL"},
	[DUNE_SYS_UTIMES] = {NULL, "DUNE_SYS_UTIMES"},
	[DUNE_SYS_VSERVER] = {NULL, "DUNE_SYS_VSERVER"},
	[DUNE_SYS_MBIND] = {NULL, "DUNE_SYS_MBIND"},
	[DUNE_SYS_SET_MEMPOLICY] = {NULL, "DUNE_SYS_SET_MEMPOLICY"},
	[DUNE_SYS_GET_MEMPOLICY] = {NULL, "DUNE_SYS_GET_MEMPOLICY"},
	[DUNE_SYS_MQ_OPEN] = {NULL, "DUNE_SYS_MQ_OPEN"},
	[DUNE_SYS_MQ_UNLINK] = {NULL, "DUNE_SYS_MQ_UNLINK"},
	[DUNE_SYS_MQ_TIMEDSEND] = {NULL, "DUNE_SYS_MQ_TIMEDSEND"},
	[DUNE_SYS_MQ_TIMEDRECEIVE] = {NULL, "DUNE_SYS_MQ_TIMEDRECEIVE"},
	[DUNE_SYS_MQ_NOTIFY] = {NULL, "DUNE_SYS_MQ_NOTIFY"},
	[DUNE_SYS_MQ_GETSETATTR] = {NULL, "DUNE_SYS_MQ_GETSETATTR"},
	[DUNE_SYS_KEXEC_LOAD] = {NULL, "DUNE_SYS_KEXEC_LOAD"},
	[DUNE_SYS_WAITID] = {NULL, "DUNE_SYS_WAITID"},
	[DUNE_SYS_ADD_KEY] = {NULL, "DUNE_SYS_ADD_KEY"},
	[DUNE_SYS_REQUEST_KEY] = {NULL, "DUNE_SYS_REQUEST_KEY"},
	[DUNE_SYS_KEYCTL] = {NULL, "DUNE_SYS_KEYCTL"},
	[DUNE_SYS_IOPRIO_SET] = {NULL, "DUNE_SYS_IOPRIO_SET"},
	[DUNE_SYS_IOPRIO_GET] = {NULL, "DUNE_SYS_IOPRIO_GET"},
	[DUNE_SYS_INOTIFY_INIT] = {NULL, "DUNE_SYS_INOTIFY_INIT"},
	[DUNE_SYS_INOTIFY_ADD_WATCH] = {NULL, "DUNE_SYS_INOTIFY_ADD_WATCH"},
	[DUNE_SYS_INOTIFY_RM_WATCH] = {NULL, "DUNE_SYS_INOTIFY_RM_WATCH"},
	[DUNE_SYS_MIGRATE_PAGES] = {NULL, "DUNE_SYS_MIGRATE_PAGES"},
	[DUNE_SYS_OPENAT] = {dune_sys_openat, "DUNE_SYS_OPENAT"},
	[DUNE_SYS_MKDIRAT] = {NULL, "DUNE_SYS_MKDIRAT"},
	[DUNE_SYS_MKNODAT] = {NULL, "DUNE_SYS_MKNODAT"},
	[DUNE_SYS_FCHOWNAT] = {NULL, "DUNE_SYS_FCHOWNAT"},
	[DUNE_SYS_FUTIMESAT] = {NULL, "DUNE_SYS_FUTIMESAT"},
	[DUNE_SYS_NEWFSTATAT] = {NULL, "DUNE_SYS_NEWFSTATAT"},
	[DUNE_SYS_UNLINKAT] = {dune_sys_unlinkat, "DUNE_SYS_UNLINKAT"},
	[DUNE_SYS_RENAMEAT] = {NULL, "DUNE_SYS_RENAMEAT"},
	[DUNE_SYS_LINKAT] = {NULL, "DUNE_SYS_LINKAT"},
	[DUNE_SYS_SYMLINKAT] = {NULL, "DUNE_SYS_SYMLINKAT"},
	[DUNE_SYS_READLINKAT] = {dune_sys_readlinkat, "DUNE_SYS_READLINKAT"},
	[DUNE_SYS_FCHMODAT] = {NULL, "DUNE_SYS_FCHMODAT"},
	[DUNE_SYS_FACCESSAT] = {NULL, "DUNE_SYS_FACCESSAT"},
	[DUNE_SYS_PSELECT6] = {dune_sys_pselect6, "DUNE_SYS_PSELECT6"},
	[DUNE_SYS_PPOLL] = {NULL, "DUNE_SYS_PPOLL"},
	[DUNE_SYS_UNSHARE] = {NULL, "DUNE_SYS_UNSHARE"},
	[DUNE_SYS_SET_ROBUST_LIST] = {NULL, "DUNE_SYS_SET_ROBUST_LIST"},
	[DUNE_SYS_GET_ROBUST_LIST] = {NULL, "DUNE_SYS_GET_ROBUST_LIST"},
	[DUNE_SYS_SPLICE] = {NULL, "DUNE_SYS_SPLICE"},
	[DUNE_SYS_TEE] = {NULL, "DUNE_SYS_TEE"},
	[DUNE_SYS_SYNC_FILE_RANGE] = {NULL, "DUNE_SYS_SYNC_FILE_RANGE"},
	[DUNE_SYS_VMSPLICE] = {NULL, "DUNE_SYS_VMSPLICE"},
	[DUNE_SYS_MOVE_PAGES] = {NULL, "DUNE_SYS_MOVE_PAGES"},
	[DUNE_SYS_UTIMENSAT] = {NULL, "DUNE_SYS_UTIMENSAT"},
	[DUNE_SYS_EPOLL_PWAIT] = {NULL, "DUNE_SYS_EPOLL_PWAIT"},
	[DUNE_SYS_SIGNALFD] = {NULL, "DUNE_SYS_SIGNALFD"},
	[DUNE_SYS_TIMERFD_CREATE] = {NULL, "DUNE_SYS_TIMERFD_CREATE"},
	[DUNE_SYS_EVENTFD] = {NULL, "DUNE_SYS_EVENTFD"},
	[DUNE_SYS_FALLOCATE] = {dune_sys_fallocate, "DUNE_SYS_FALLOCATE"},
	[DUNE_SYS_TIMERFD_SETTIME] = {NULL, "DUNE_SYS_TIMERFD_SETTIME"},
	[DUNE_SYS_TIMERFD_GETTIME] = {NULL, "DUNE_SYS_TIMERFD_GETTIME"},
	[DUNE_SYS_ACCEPT4] = {NULL, "DUNE_SYS_ACCEPT4"},
	[DUNE_SYS_SIGNALFD4] = {NULL, "DUNE_SYS_SIGNALFD4"},
	[DUNE_SYS_EVENTFD2] = {NULL, "DUNE_SYS_EVENTFD2"},
	[DUNE_SYS_EPOLL_CREATE1] = {dune_sys_epoll_create1,
	                            "DUNE_SYS_EPOLL_CREATE1"},
	[DUNE_SYS_DUP3] = {NULL, "DUNE_SYS_DUP3"},
	[DUNE_SYS_PIPE2] = {NULL, "DUNE_SYS_PIPE2"},
	[DUNE_SYS_INOTIFY_INIT1] = {NULL, "DUNE_SYS_INOTIFY_INIT1"},
	[DUNE_SYS_PREADV] = {NULL, "DUNE_SYS_PREADV"},
	[DUNE_SYS_PWRITEV] = {NULL, "DUNE_SYS_PWRITEV"},
	[DUNE_SYS_RT_TGSIGQUEUEINFO] = {NULL, "DUNE_SYS_RT_TGSIGQUEUEINFO"},
	[DUNE_SYS_PERF_EVENT_OPEN] = {NULL, "DUNE_SYS_PERF_EVENT_OPEN"},
	[DUNE_SYS_RECVMMSG] = {NULL, "DUNE_SYS_RECVMMSG"},
	[DUNE_SYS_FANOTIFY_INIT] = {NULL, "DUNE_SYS_FANOTIFY_INIT"},
	[DUNE_SYS_FANOTIFY_MARK] = {NULL, "DUNE_SYS_FANOTIFY_MARK"},
	[DUNE_SYS_PRLIMIT64] = {NULL, "DUNE_SYS_PRLIMIT64"},
	[DUNE_SYS_NAME_TO_HANDLE_AT] = {NULL, "DUNE_SYS_NAME_TO_HANDLE_AT"},
	[DUNE_SYS_OPEN_BY_HANDLE_AT] = {NULL, "DUNE_SYS_OPEN_BY_HANDLE_AT"},
	[DUNE_SYS_CLOCK_ADJTIME] = {NULL, "DUNE_SYS_CLOCK_ADJTIME"},
	[DUNE_SYS_SYNCFS] = {NULL, "DUNE_SYS_SYNCFS"},
	[DUNE_SYS_SENDMMSG] = {NULL, "DUNE_SYS_SENDMMSG"},
	[DUNE_SYS_SETNS] = {NULL, "DUNE_SYS_SETNS"},
	[DUNE_SYS_GETCPU] = {NULL, "DUNE_SYS_GETCPU"},
	[DUNE_SYS_PROCESS_VM_READV] = {NULL, "DUNE_SYS_PROCESS_VM_READV"},
	[DUNE_SYS_PROCESS_VM_WRITEV] = {NULL, "DUNE_SYS_PROCESS_VM_WRITEV"},
	[DUNE_SYS_KCMP] = {NULL, "DUNE_KCMP"},
	[DUNE_SYS_FINIT_MODULE] = {NULL, "DUNE_SYS_FINIT_MODULE"},
	[DUNE_SYS_SCHED_SETATTR] = {NULL, "DUNE_SYS_SCHED_SETATTR"},
	[DUNE_SYS_SCHED_GETATTR] = {NULL, "DUNE_SYS_SCHED_GETATTR"},
	[DUNE_SYS_RENAMEAT2] = {NULL, "DUNE_SYS_RENAMEAT2"},
	[DUNE_SYS_SECCOMP] = {NULL, "DUNE_SYS_SECCOMP"},
	[DUNE_SYS_GETRANDOM] = {dune_sys_getrandom, "DUNE_SYS_GETRANDOM"},
	[DUNE_SYS_MEMFD_CREATE] = {NULL, "DUNE_SYS_MEMFD_CREATE"},
	[DUNE_SYS_KEXEC_FILE_LOAD] = {NULL, "DUNE_SYS_KEXEC_FILE_LOAD"},
	[DUNE_SYS_BPF] = {NULL, "DUNE_SYS_BPF"},
	[DUNE_STUB_EXECVEAT] = {NULL, "DUNE_STUB_EXECVEAT"},
	[DUNE_USERFAULTFD] = {NULL, "DUNE_USERFAULTFD"},
	[DUNE_MEMBARRIER] = {NULL, "DUNE_MEMBARRIER"},
	[DUNE_MLOCK2] = {NULL, "DUNE_MLOCK2"},
	[DUNE_COPY_FILE_RANGE] = {NULL, "DUNE_COPY_FILE_RANGE"},
	[DUNE_PREADV2] = {NULL, "DUNE_PREADV2"},
	[DUNE_PWRITEV2] = {NULL, "DUNE_PWRITEV2"},

};

bool init_syscall_table(void)
{
	int i;

	for (i = 0; i < DUNE_MAX_NUM_SYSCALLS ; ++i) {
		if (dune_syscall_table[i].name == NULL)
			dune_syscall_table[i].name = "nosyscall";
	}

	if (dlopen("liblinuxemu_extend.so", RTLD_NOW) == NULL) {
		fprintf(stderr, "Not using any syscall extensions\n Reason: %s\n",
		        dlerror());
		return false;
	}

	return true;
}

void lemuprint(const uint32_t tid, uint64_t syscall_number,
               const bool isError, const char *fmt, ...)
{
	va_list valist;
	const char *prefix = "[TID %d] %s: ";
	bool double_logging = false;

	// Do not use global variable as a check to acquire lock.
	// make sure it is not changed during our acquire/release.
	int debug = lemu_debug;

	// If we are not going to log anything anyway, just bail out.
	if (!(debug > 0 || isError))
		return;

	const char *syscall_name;

	if (syscall_number >= DUNE_MAX_NUM_SYSCALLS)
		panic("lemuprint: Illegal Syscall #%d!\n", syscall_number);
	else
		syscall_name = dune_syscall_table[syscall_number].name;

	va_start(valist, fmt);

	uth_mutex_lock(lemu_logging_lock);

	// Print to stderr if debug level is sufficient
	if (debug > 1) {
		fprintf(stderr, prefix, tid, syscall_name);
		vfprintf(stderr, fmt, valist);
		// Checks if we will double log to stderr
		if (lemu_global_logfile == stderr)
			double_logging = true;
	}

	// Log to the global logfile, if we defaulted the global logging to
	// stderr then we don't want to log 2 times to stderr.
	if (lemu_global_logfile != NULL && !double_logging) {
		fprintf(lemu_global_logfile, prefix, tid, syscall_name);
		vfprintf(lemu_global_logfile, fmt, valist);
	}

	uth_mutex_unlock(lemu_logging_lock);

	va_end(valist);
}


/* TODO: have an array which classifies syscall args
 * and "special" system calls (ones with weird return
 * values etc.). For some cases, we don't even do a system
 * call, and in many cases we have to rearrange arguments
 * since Linux and Akaros don't share signatures, so this
 * gets tricky. */
bool
linuxemu(struct guest_thread *gth, struct vm_trapframe *tf)
{
	bool ret = false;

	if (tf->tf_rax >= DUNE_MAX_NUM_SYSCALLS) {
		fprintf(stderr, "System call %d is out of range\n", tf->tf_rax);
		return false;
	}


	if (dune_syscall_table[tf->tf_rax].call == NULL) {
		fprintf(stderr, "System call #%d (%s) is not implemented\n",
		        tf->tf_rax, dune_syscall_table[tf->tf_rax].name);
		return false;
	}

	lemuprint(tf->tf_guest_pcoreid, tf->tf_rax,
	          false, "vmcall(%d, %p, %p, %p, %p, %p, %p);\n", tf->tf_rax,
	          tf->tf_rdi, tf->tf_rsi, tf->tf_rdx, tf->tf_r10, tf->tf_r8,
	          tf->tf_r9);

	tf->tf_rip += 3;

	return (dune_syscall_table[tf->tf_rax].call)(tf);
}