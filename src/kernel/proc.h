#pragma once

#include <common/defines.h>
#include <common/list.h>
#include <common/sem.h>
#include <common/rbtree.h>
#include <kernel/pt.h>

enum procstate { UNUSED, RUNNABLE, RUNNING, SLEEPING, DEEPSLEEPING, ZOMBIE };

typedef struct UserContext {
    // TODO: customize your trap frame
    u64 spsr, elr, lr, sp;
    u64 x[18];
} UserContext;

typedef struct KernelContext {
    // TODO: customize your context
    u64 lr, x0, x1;
    u64 x[11]; // x19-29
} KernelContext;

// embeded data for procs
struct schinfo {
    // TODO: customize your sched info
    ListNode rq;
};

typedef struct Proc {
    bool killed;
    bool idle;
    int pid;
    int exitcode;
    enum procstate state;
    Semaphore childexit;
    ListNode children;
    ListNode ptnode;
    struct Proc *parent;
    struct schinfo schinfo;
    struct pgdir pgdir;
    void *kstack;
    UserContext *ucontext;
    KernelContext *kcontext;
} Proc;

void init_kproc();
void init_proc(Proc *);
WARN_RESULT Proc *create_proc();
int start_proc(Proc *, void (*entry)(u64), u64 arg);
NO_RETURN void exit(int code);
WARN_RESULT int wait(int *exitcode);
WARN_RESULT int kill(int pid);



typedef struct pid_node {
    int pid;
    ListNode node;
} PIDNode;

typedef struct pid_manager {
    int max_pid;
    PIDNode freepid;
} PIDManager;

void init_pidmanager(PIDManager* manager);
int get_pid(PIDManager* manager);
void reuse_pid(PIDManager* manager, int pid);
