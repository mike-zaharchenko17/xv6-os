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


