### How to Run ###

1. boot up codespace on branch hw3/scheduler
2. configure your environment with [the class guide](https://docs.google.com/document/d/1bsPGZdJ27jDAH2IKA29RQUk6ilfo6pCevHgAw4bZap8/edit?tab=t.0)
3. to run with scheduler
    - don't touch the makefile
4. to run without scheduler
    - in Makefile, comment out line 81 (CFLAGS += -DPRIORITY_SCHED)
5. run the following
    - make clean
    - make qemu-nox

### nice ###

To set up the nice system call:

- (proc.h) defined bounds for NICE and PRIORITY
    - min and max for each set to 0 and 4, respectively
    - lower nice value = higher priority; 1:1 mapping
    - this, a priority of 0 is highest priority and a priority of 4 is lowest priority.
- (proc.h) defined inline helpers
    - clamp_integer(int, int, int)
        - if value is higher than max or lower than min, return max or min
        - otherwise return value
        - this handles the out of bounds case
    - priority_from_nice(int)
        - basically just clamps the proposed value and returns it, which works since it's a 1:1 mapping
- (proc.c) modified allocproc(void)
    - since allocproc is a helper used in all of our process "spawners" to allocate stack space, it made sense to handle default nice and priority values here
    - after the kernel stack is allocated and context is set, we set the process's default nice and default priority to 2
- (syscall.h) added syscall declaration for nice
- (sysproc.c) added find_proc_locked helper
    - basically just performs a linear search- nothing fancy here
- (syscall.c) declared sys_nice
- (user.h & usys.S) more declarations; piping all this to userland
- nice.c
    - driver for nice
    - parses string args with atoi
    - case 1: 2 args `nice <pid> <value>`
        - sets nice for the specified pid
        - saves old nice
        - handles errors
        - prints old nice
    - case 2: 1 arg `nice <value>`
        - retrieves pid with getpid helper
        - sets nice for the retrieved pid
        - saves old nice
        - prints old nice

### scheduler ###

The scheduler uses an array of priority buckets, each containing a double ended queue where the priority level corresponds to index i.

[
    i=0: [*]<->[*]
    i=1: [*]<->[*]<->[*]<->[*]
    i=2: [*]<->[*]<->[*]<->[*]<->[*]<->[*]<->[*]
    i=3: [*]<->[*]<->[*]
    i=4: [*]<->[*]<->[*]<->[*]<->[*]<->[*]
]

To make this work, I added the following to the proc struct (proc.h)
    - proc *q_prev
    - proc *q_next

These are intrusive links. They effectively make each process a node in the DE queues and mean you don't have to malloc a queue node for each process. This gives us O(1) enqueue and dequeue and O(n) lookup.

The structures that make up this queue, as well as helper methods that enable the queue to function are defined in proc.c

syscalls modified

### Tests ###

Test helpers are defined at testhelpers.h

- yieldn(int)
    - yields n times
- burn(int)
    - busy waits for the number of ticks specified in the argument

**hw3test1.c**

The goal of this test is to get roughly alternating input between children. The children are both spawned with the default priority. Therefore, one should not overtake the other.











