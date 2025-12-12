.globl thread_switch
.type thread_switch, @function
thread_switch:
    # save callee-saved registers on current stack
    pushl %ebp
    pushl %ebx
    pushl %esi
    pushl %edi

    # args are now at:
    # old = 4(%esp) + 16 = 20(%esp)
    # next = 8(%esp) + 16 = 24(%esp)

    movl 20(%esp), %eax
    movl 24(%esp), %edx

    # save current stack ptr into old->sp (sp is at offset 0)
    movl %esp, 0(%eax)

    # switch stacks
    movl 0(%edx), %esp

    # restore callee-saved registers
    popl %edi
    popl %esi
    popl %ebx
    popl %ebp

    ret

.size thread_switch, .-thread_switch