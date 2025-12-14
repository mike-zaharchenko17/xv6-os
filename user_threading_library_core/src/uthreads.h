#include "types.h"

#define MAX_THREADS 32
#define STACK_SIZE 8192

enum threadstate { T_UNUSED, T_RUNNABLE, T_RUNNING, T_SLEEPING, T_ZOMBIE };

// thread structure

/*
invariants:

- Exactly one thread is T_RUNNING at a time
- A thread in T_ZOMVIE keeps its retval until join consumes it,
  then slot becomes T_UNUSED

*/
struct thread {
    uint sp; // stack ptr
    int tid;
    enum threadstate tstate;

    char *stack; // base of stack

    // so the thread knows what to run
    void *(*start_routine)(void *);
    void *arg;

    // return value for join
    void *retval;

    int joiner_tid; //-1 if none

    struct thread *qnext;
};

// thread table
extern struct thread threads[MAX_THREADS];

// current thread ptr
extern struct thread *current_thread;

// for allocating TIDs
extern int next_tid;

// API methods

/*

thread_init

Overview:
Initializes the threading system. This must be the first function a user calls.

Must do:
1.  Initialize global state (particularly the thread table)
2.  Account for the fact that the main program is already running and
    must be set up as the first thread (thread 0) in the T_RUNNING state

*/

void thread_init(void);

/*

thread_create

Overview: 
Creates a new thread that will execute the start_routine function, 
passing arg as its only parameter.

Must do:
1.  Find an unused thread slot
2.  Set its state to T_RUNNABLE
3.  Initialize its stack

The stack setup must ensure that when the scheduler first switches to 
this thread, it begins by calling the start_routine function with its 
argument, and when that function returns, the thread automatically 
calls thread_exit() with the return value.

*/
int thread_create(void *(*start_routine)(void *), void *arg);

/*

thread_join

Overview:
Waits for the thread specified by tid to terminate.

Must do:
1.  If the target trhread is not yet finished, the calling thread must block
    until the target thread exits
        a) i.e., set its own state to T_SLEEPING until target thread exits

2.  Once the target is T_ZOMBIE, this function should clean up its resources
        a) i.e., set its state to T_UNUSED and collect its return value

*/
void *thread_join(int tid);

/*

thread_exit

Overview:
Terminates the currently running thread.

Must do:
1.  Save the retval so it can be collected by a joining thread
2.  Set the thread's state to T_ZOMBIE and wake up any other thread that
    may be thread_join-ing on it (by setting that thread's state to T_RUNNABLE)

This function does not return. It must call the scheduler to run a new thread.

*/

void thread_exit(void *retval);

/*

thread_self

Overview: returns the tid of the currently running thread

*/

int thread_self(void);

/*

thread_yield

Overview:
Voluntarily gives up the CPU to allow other threads to run

Must do:
The current thread should be marked as T_RUNNABLE and the scheduler should
be called to select a new thread to run

*/

void thread_yield(void);

/****** SYNC PRIMITIVES *****/

// MUTEX

typedef struct mutex {
    // 1: locked, 0: unlocked
    int locked;
    // since threads have a qnext attribute, we can just keep track of the head
    struct thread *qhead; 
    struct thread *qtail;
    struct thread *owner;
} mutex_t;

/*

mutex_init

Overview: initializes a mutex_t struct before its first use
Must do: set mutex's internal state to "unlocked"

*/

void mutex_init(mutex_t *m);

/*

mutex lock

Overview: acquires the mutex for the currently-running thread

Must do:

1.  if mutex is unlocked, the function should mark it as "locked" and return immediately
    a)  it should also track the owner

2.  if the mutex is already locked by another thread, the function must "block"
    a)  in other words, it must set the current thread's state to T_SLEEPING, add it to the mutex's
        wait queue, and call thread_schedule() to run another thread

*/

void mutex_lock(mutex_t *m);

/*

mutex_unlock

Overview: releases the mutex held by the currently-running thread

Must do:

1.  First, verify that the currently running thread is the one that holds the lock
2.  Then, check if any other threads are waiting in its queue
3.  If no threads are waiting, it simply marks the mutex as "unlocked"
4.  IF threads ARE waiting, it must wake one of them up by removing it from the wait queue
    and setting its state to T_RUNNABLE

*/

void mutex_unlock(mutex_t *m);



