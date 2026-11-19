    .text
    .hidden vecdot_count
    .hidden vecdot_tramp
    .globl vecdot_count_stub
    .type vecdot_count_stub,@function
vecdot_count_stub:
    incq vecdot_count(%rip)
    jmp *vecdot_tramp(%rip)
