#define MAX_THREADS 32;
#define STACK_SIZE 8192;

enum threadstate { T_UNUSED, T_RUNNABLE, T_RUNNING, T_SLEEPING, T_ZOMBIE };

// thread structure
struct thread {
    int tid;
    enum threadstate tstate;

    char *stack; // base of stack
    uint sp; // stack ptr
};

// thread table
struct thread threads[MAX_THREADS];

// current thread ptr
struct thread *current_thread;

// for allocating TIDs
int next_tid = 1;



