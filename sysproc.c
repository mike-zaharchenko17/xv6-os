#include "types.h"
#include "x86.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "spinlock.h"


// tell compiler ptable exists in another file
extern struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

int
sys_fork(void)
{
  return fork();
}

int
sys_exit(void)
{
  exit();
  return 0;  // not reached
}

int
sys_wait(void)
{
  return wait();
}

int
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

int
sys_getpid(void)
{
  return myproc()->pid;
}

int
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

int
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

// return how many clock tick interrupts have occurred
// since start.
int
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

// find process; use only with lock acquired
static struct proc* find_proc_locked(int pid) {
  struct proc *p;
  for (p = ptable.proc; p < &ptable.proc[NPROC]; p++) {
    if (p->pid == pid) {
      return p;
    }
  }
  return 0;
}

int sys_nice(void) {
  int pid, val;

  if (argint(0, &pid) < 0 || argint(1, &val) < 0) {
    return -1;
  }

  acquire(&ptable.lock);

  // find the process; safe bc lock acquired
  struct proc *p = find_proc_locked(pid);

  // if not found, release lock, return err
  if (!p) {
    release(&ptable.lock);
    return -1;
  }

  // copy over the old nice value
  int old = p->nice;

  p->nice = clamp_integer(val, NICE_MIN, NICE_MAX);

  p->priority = priority_from_nice(p->nice);

  release(&ptable.lock);

  return old;
}
