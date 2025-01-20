#include <kernel/syscall.h>
#include <kernel/sched.h>
#include <kernel/printk.h>
#include <common/sem.h>
#include <test/test.h>
#include <aarch64/intrinsic.h>
#include <kernel/paging.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"

void init_syscall()
{
    for (u64 *p = (u64 *)&early_init; p < (u64 *)&rest_init; p++)
        ((void (*)()) * p)();
}

void *syscall_table[NR_SYSCALL] = {
    [0 ... NR_SYSCALL - 1] = NULL,
    [SYS_myreport] = (void *)syscall_myreport,
};

void syscall_entry(UserContext *context)
{
    // TODO
    // Invoke syscall_table[id] with args and set the return value.
    // id is stored in x8. args are stored in x0-x5. return value is stored in x0.
    // be sure to check the range of id. if id >= NR_SYSCALL, panic.
    u64 id = context->x[8];
    u64 ret = 0;
    if(id >= NR_SYSCALL){
        PANIC();
    }
    else if(syscall_table[id] != NULL){
        ret = ((u64(*)(u64, u64, u64, u64, u64, u64))syscall_table[id])(context->x[0], context->x[1], context->x[2], context->x[3], context->x[4], context->x[5]);
        context->x[0] = ret;
    }
}

/** 
 * Check if the virtual address [start,start+size) is READABLE by the current
 * user process.
 */
bool user_readable(const void *start, usize size) {
    /* (Final) TODO BEGIN */
    start = start;
    size = size;
    return true;
    /* (Final) TODO END */
}


/**
 * Check if the virtual address [start,start+size) is READABLE & WRITEABLE by
 * the current user process.
 */
bool user_writeable(const void *start, usize size) {
    /* (Final) TODO Begin */
    bool ret = true;
    for(u64 i = (u64)start; i < (u64)start + size; i = (i / BLOCK_SIZE + 1) * BLOCK_SIZE){
        auto pte = get_pte(&thisproc()->pgdir, i, false);
        if(pte == NULL || ((*pte) & PTE_RO)){
            ret = false;
            break;
        }
    }
    return ret;
    /* (Final) TODO End */
}

/** 
 * Get the length of a string including tailing '\0' in the memory space of
 * current user process return 0 if the length exceeds maxlen or the string is
 * not readable by the current user process.
 */
usize user_strlen(const char *str, usize maxlen) {
    for (usize i = 0; i < maxlen; i++) {
        if (user_readable(&str[i], 1)) {
            if (str[i] == 0)
                return i + 1;
        } else
            return 0;
    }
    return 0;
}