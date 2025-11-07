#define PRIORITY_MIN 0
#define PRIORITY_MAX 4
#define NICE_MIN 0
#define NICE_MAX 4
#define PRIORITY_LEVELS 5

// x, low, high
static inline int clamp_integer(int x, int l, int h) {
  if (x < l) {
    return l;
  }
  if (x > h) {
    return h;
  }
  return x;
}

static inline int priority_from_nice(int nice_val) {
  return clamp_integer(nice_val, NICE_MIN, NICE_MAX);
}

// Per-CPU state
struct cpu {
  uchar apicid;                // Local APIC ID
  struct context *scheduler;   // swtch() here to enter scheduler
  struct taskstate ts;         // Used by x86 to find stack for interrupt
  struct segdesc gdt[NSEGS];   // x86 global descriptor table
  volatile uint started;       // Has the CPU started?
  int ncli;                    // Depth of pushcli nesting.
  int intena;                  // Were interrupts enabled before pushcli?
  struct proc *proc;           // The process running on this cpu or null
  struct proc *q_prev;         // The preceding process in the run queue
  struct proc *q_next;         // The next process in the run queue (DLL)
};

extern struct cpu cpus[NCPU];
extern int ncpu;

//PAGEBREAK: 17
// Saved registers for kernel context switches.
// Don't need to save all the segment registers (%cs, etc),
// because they are constant across kernel contexts.
// Don't need to save %eax, %ecx, %edx, because the
// x86 convention is that the caller has saved them.
// Contexts are stored at the bottom of the stack they
// describe; the stack pointer is the address of the context.
// The layout of the context matches the layout of the stack in swtch.S
// at the "Switch stacks" comment. Switch doesn't save eip explicitly,
// but it is on the stack and allocproc() manipulates it.
struct context {
  uint edi;
  uint esi;
  uint ebx;
  uint ebp;
  uint eip;
};

enum procstate { UNUSED, EMBRYO, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  uint sz;                     // Size of process memory (bytes)
  pde_t* pgdir;                // Page table
  char *kstack;                // Bottom of kernel stack for this process
  enum procstate state;        // Process state
  int pid;                     // Process ID
  struct proc *parent;         // Parent process
  struct trapframe *tf;        // Trap frame for current syscall
  struct context *context;     // swtch() here to run process
  void *chan;                  // If non-zero, sleeping on chan
  int nice;                    // 0-4 nice value
  int priority;                // 0-4 scheduler priorty; derived from nice
  int killed;                  // If non-zero, have been killed
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
};

// Process memory is laid out contiguously, low addresses first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap

// we're creating a doubly-linked queue (LL implementation) of the processes
// with 'buckets' at each priority level

struct run_queue {
  struct proc *head
  struct proc *tail
  int length;
};

// The ready_queues structure is an array of run queues where index i corresponds
// to priority level i

/*
  [
    i=0: [*]<->[*]
    i=1: [*]<->[*]<->[*]<->[*]<->[*]
    i=2: [*]<->[*]<->[*]<->[*]<->[*]<->[*]<->[*]
    i=3: [*]<->[*]<->[*]
    i=4: [*]<->[*]<->[*]<->[*]<->[*]<->[*]
  ]

  something like this^, but the 'pX:' does not symbolize a k/v pair
*/

static struct run_queue ready_queues[PRIORITY_LEVELS];

static void rq_push_tail_locked(int level, struct proc *p) {
  if (!holding(&ptable.lock)) {
    panic("rq_push_tail_locked: ptable.lock not held");
  }

  struct run_queue *q = &ready_queues[level];

  p->q_prev = q->tail;
  p->q_next = 0;

  // if tail is not null (queue not empty) then set current tail's
  // next ptr to point to p
  // otherwise set head to p
  if (q->tail != 0) { 
    q->tail->q_next = p;
  } else {
    // if empty: head = p
    q->head = p;
  }

  // p is the new tail
  q->tail = p;
  // grow queue
  q->length++;
}

static void rq_remove_locked(struct proc *p) {
  if (!holding(&ptable.lock)) {
    panic("rq_remove_locked: ptable.lock not held");
  }

  int level = p->priority;
  struct run_queue *q = &ready_queues[level];

  if (p->q_prev != 0) {
    p->q_prev->q_next = p->q_next;
  } else {
    q->head = p->q_next;
  }

  if (p->q_next != 0) {
    p->q_next->q_prev = p->q_prev;
  } else {
    q->tail = p->q_prev;
  }

  p->q_prev = 0;
  p->q_next = 0;
  q->length--;
}

static struct proc * rq_pop_head_locked(int level) {
  if (!holding(&ptable.lock)) {
    panic("rq_pop_head_locked: ptable.lock not held");
  }

  struct run_queue *q = &ready_queues[level];

  struct proc *p = q->head;

  if (p == 0) {
    return 0;
  }

  q->head = p->q_next;

  if (q->head != 0) {
    // proc -> q_prev
    q->head->q_prev = 0;
  } else {
    q->tail = 0;
  }

  p->q_prev = 0;
  p->q_next = 0;
  q->length--;

  return p;
}