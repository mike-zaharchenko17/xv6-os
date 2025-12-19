.globl thread_switch
.type thread_switch, @function
thread_switch:
    # save callee-saved registers on current stack
    # We push these because the C calling convention requires them to be preserved.
    # The caller expects these registers to be the same when we return.
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    # args are now at:
    # old = 4(%esp) + 16 (4 pushes * 4 bytes) = 20(%esp)
    # next = 8(%esp) + 16 (4 pushes * 4 bytes) = 24(%esp)

    movl 20(%esp), %eax # eax = struct thread *old
    movl 24(%esp), %edx # edx = struct thread *next

    # save current stack ptr into old->sp (sp is at offset 0 in struct thread)
    movl %esp, 0(%eax)

    # switch stacks!
    # Load the stack pointer of the 'next' thread.
    movl 0(%edx), %esp

    # restore callee-saved registers from the NEW stack
    # These were pushed when 'next' was switched out previously.
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp

    # return to the address on the NEW stack
    ret

.size thread_switch, .-thread_switch