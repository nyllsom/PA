#include <common.h>
#include "syscall.h"
#include "amdev.h"
#include "klib-macros.h"
#include <sys/time.h>
#include <proc.h>

// #define STRACE

static const char *syscall_names[] = {
  [SYS_exit]         = "exit",
  [SYS_yield]        = "yield",
  [SYS_open]         = "open",
  [SYS_read]         = "read",
  [SYS_write]        = "write",
  [SYS_kill]         = "kill",
  [SYS_getpid]       = "getpid",
  [SYS_close]        = "close",
  [SYS_lseek]        = "lseek",
  [SYS_brk]          = "brk",
  [SYS_fstat]        = "fstat",
  [SYS_time]         = "time",
  [SYS_signal]       = "signal",
  [SYS_execve]       = "execve",
  [SYS_fork]         = "fork",
  [SYS_link]         = "link",
  [SYS_unlink]       = "unlink",
  [SYS_wait]         = "wait",
  [SYS_times]        = "times",
  [SYS_gettimeofday] = "gettimeofday",
};

extern int fs_open(const char *pathname, int flags, int mode);
extern size_t fs_read(int fd, void *buf, size_t len);
extern size_t fs_write(int fd, const void *buf, size_t len);
extern size_t fs_lseek(int fd, size_t offset, int whence);
extern int fs_close(int fd);
extern void naive_uload(PCB *pcb, const char *filename);

void strace(Context *c, uintptr_t sys_id) {
  const char *name = syscall_names[sys_id];
  if (!name) name = "unknown_syscall";

  Log("syscall %s(%d, %d, %d) GPRx // a0=%d, a1=%d, a2=%d GPRx=%d",
      name,
      (int)c->GPR2, (int)c->GPR3, (int)c->GPR4,   
      (int)c->GPR2, (int)c->GPR3, (int)c->GPR4, 
      (int)c->GPRx);
}

void sys_exit(Context *c) {
  naive_uload(NULL, "/bin/nterm"); // 或 /bin/menu
}

void sys_yield(Context *c) {
  yield();
  c->GPRx = c->GPRx;
}

void sys_open(Context *c, uintptr_t *a) {
  const char *pathname = (const char *)a[1];
  int flags = (int) a[2];
  int mode = (int) a[3];
  c->GPRx = fs_open(pathname, flags, mode);
}

void sys_read(Context *c, uintptr_t *a) {
  int fd = (int)a[1];
  void *buf = (void *)a[2];
  size_t len = (size_t)a[3];
  c->GPRx = fs_read(fd, buf, len);
}

void sys_write(Context *c, uintptr_t *a) {
  int fd = (int)a[1];
  void *buf = (void*)a[2];
  size_t len = (size_t)a[3];
  c->GPRx = fs_write(fd, buf, len);
}

void sys_close(Context *c, uintptr_t *a) {
  int fd = (int)a[1];
  c->GPRx = fs_close(fd);
}

void sys_lseek(Context *c, uintptr_t *a) {
  int fd = (int)a[1];
  size_t offset = (size_t)a[2];
  int whence = (int)a[3];
  c->GPRx = fs_lseek(fd, offset, whence);
}

void sys_brk(Context *c) {
  c->GPRx = 0;
}


static void sys_execve(Context *c) { 
  naive_uload(NULL, (const char *)c->GPR2); 
}

void sys_gettimeofday(Context *c, uintptr_t *a) {
  struct timeval *tv = (struct timeval *)a[1];
  uint64_t uptime = io_read(AM_TIMER_UPTIME).us;
  tv->tv_sec = uptime / 1000000;
  tv->tv_usec = uptime % 1000000;
  c->GPRx = 0;
}

void do_syscall(Context *c) {
  uintptr_t a[4];
  a[0] = c->GPR1; // a7
  a[1] = c->GPR2; // a0
  a[2] = c->GPR3; // a1
  a[3] = c->GPR4; // a2
  
  switch (a[0]) {

    case SYS_exit:  sys_exit(c);  break;
    case SYS_yield: sys_yield(c); break; 
    case SYS_open: sys_open(c, a); break;
    case SYS_read: sys_read(c, a); break;
    case SYS_write: sys_write(c, a); break;
    
    case SYS_kill: goto unhandled;
    case SYS_getpid: goto unhandled;

    case SYS_close: sys_close(c, a); break;
    case SYS_lseek: sys_lseek(c, a); break;    
    case SYS_brk: sys_brk(c); break;
    
    case SYS_fstat: goto unhandled;
    case SYS_time: goto unhandled;
    case SYS_signal: goto unhandled;
    
    case SYS_execve: sys_execve(c); break;
    
    case SYS_fork: goto unhandled;
    case SYS_link: goto unhandled;
    case SYS_unlink: goto unhandled;
    case SYS_wait: goto unhandled;
    case SYS_times: goto unhandled;

    case SYS_gettimeofday: sys_gettimeofday(c, a); break;

    unhandled: default: panic("Unhandled syscall ID = %d, corresponds to %s", a[0], syscall_names[a[0]]);
  }

#ifdef STRACE  
  strace(c, a[0]);
#endif
}
