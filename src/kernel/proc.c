#include <kernel/proc.h>
#include <kernel/mem.h>
#include <kernel/sched.h>
#include <aarch64/mmu.h>
#include <common/list.h>
#include <common/string.h>
#include <kernel/printk.h>

Proc root_proc;

void kernel_entry();
void proc_entry();

static SpinLock plock;
static PIDManager pmanager;

void init_pidmanager(PIDManager* manager){
    manager->max_pid = 0;
    init_list_node(&manager->freepid.node);
}

int get_pid(PIDManager* manager){
    if(_empty_list(&manager->freepid.node)){ // if no free, return a new one
        manager->max_pid++;
        return manager->max_pid; 
    }
    else{
        auto free_pidnode = container_of(manager->freepid.node.next, PIDNode, node);
        int pid = free_pidnode->pid;
        _detach_from_list(&free_pidnode->node);
        kfree(free_pidnode);
        return pid;
    }
}

void reuse_pid(PIDManager* manager, int pid){
    PIDNode* pidnode = kalloc(sizeof(PIDNode));
    pidnode->pid = pid;
    init_list_node(&pidnode->node);
    _insert_into_list(&manager->freepid.node, &pidnode->node);
}

// init_kproc initializes the kernel process
// NOTE: should call after kinit
void init_kproc()
{
    // TODO:
    // 1. init global resources (e.g. locks, semaphores)
    // 2. init the root_proc (finished)
    init_spinlock(&plock);
    init_pidmanager(&pmanager);

    init_proc(&root_proc);
    root_proc.parent = &root_proc;
    start_proc(&root_proc, kernel_entry, 123456);
}

void init_proc(Proc *p)
{
    // TODO:
    // setup the Proc with kstack and pid allocated
    // NOTE: be careful of concurrency
    acquire_spinlock(&plock);
    memset(p, 0, sizeof(Proc));
    p->pid = get_pid(&pmanager);
    init_sem(&p->childexit, 0);
    init_list_node(&p->children);
    init_list_node(&p->ptnode);
    p->killed = 0;
    p->idle = 0;
    p->parent = NULL;
    p->kstack = kalloc_page();
    init_schinfo(&p->schinfo);
    init_pgdir(&p->pgdir);
    p->kcontext = (KernelContext*)((u64)p->kstack + PAGE_SIZE - 16 - sizeof(KernelContext) - sizeof(UserContext));
    p->ucontext = (UserContext*)((u64)p->kstack + PAGE_SIZE - 16 - sizeof(UserContext));
    release_spinlock(&plock);
}

Proc *create_proc()
{
    Proc *p = kalloc(sizeof(Proc));
    init_proc(p);
    return p;
}

void set_parent_to_this(Proc *proc)
{
    // TODO: set the parent of proc to thisproc
    // NOTE: maybe you need to lock the process tree
    // NOTE: it's ensured that the old proc->parent = NULL
    acquire_spinlock(&plock);
    proc->parent = thisproc();
    _insert_into_list(&thisproc()->children, &proc->ptnode);
    release_spinlock(&plock);
}

int start_proc(Proc *p, void (*entry)(u64), u64 arg)
{
    // TODO:
    // 1. set the parent to root_proc if NULL
    // 2. setup the kcontext to make the proc start with proc_entry(entry, arg)
    // 3. activate the proc and return its pid
    // NOTE: be careful of concurrency
    acquire_spinlock(&plock);
    if(p->parent == NULL){
        p->parent = &root_proc;
        _insert_into_list(&root_proc.children, &p->ptnode);
    }
    release_spinlock(&plock);
    p->kcontext->lr = (u64)&proc_entry;
    p->kcontext->x0 = (u64)entry;
    p->kcontext->x1 = (u64)arg;
    int id = p->pid;
    activate_proc(p);
    return id;
}

int wait(int *exitcode)
{
    // TODO:
    // 1. return -1 if no children
    // 2. wait for childexit
    // 3. if any child exits, clean it up and return its pid and exitcode
    // NOTE: be careful of concurrency
    auto this = thisproc();
    acquire_spinlock(&plock);
    if(this->children.next == &this->children){
        release_spinlock(&plock);
        return -1;
    }
    release_spinlock(&plock);
    if(!wait_sem(&this->childexit)){
        return -1;
    }
    acquire_spinlock(&plock);
    acquire_sched_lock();
    Proc* zombienode = NULL;
    _for_in_list(p, &this->children){
        if(p == &this->children){
            continue;
        }
        auto child = container_of(p, Proc, ptnode);
        if(child->state == ZOMBIE){
            zombienode = child;
            break;
        }
    }
    if(zombienode != NULL){
        ASSERT(zombienode->state == ZOMBIE);
        _detach_from_list(&zombienode->ptnode);
        _detach_from_list(&zombienode->schinfo.rq);
        *exitcode = zombienode->exitcode;
        kfree_page(zombienode->kstack);
        int ret_pid = zombienode->pid;
        reuse_pid(&pmanager, zombienode->pid);
        kfree(zombienode);
        release_sched_lock();
        release_spinlock(&plock);
        return ret_pid;
    }
    release_sched_lock();
    release_spinlock(&plock);
    return -1;
}

NO_RETURN void exit(int code)
{
    // TODO:
    // 1. set the exitcode
    // 2. clean up the resources
    // 3. transfer children to the root_proc, and notify the root_proc if there is zombie
    // 4. sched(ZOMBIE)
    // NOTE: be careful of concurrency
    acquire_spinlock(&plock);
    acquire_sched_lock();
    auto this = thisproc();
    this->exitcode = code;
    int times = 0;
    _for_in_list(p, &this->children){
        if(p == &this->children){
            continue;
        }
        auto child = container_of(p, Proc, ptnode);
        child->parent = &root_proc;
        if(child->state == ZOMBIE){
            times++;
        }
    }
    if(!_empty_list(&this->children)){
        _merge_list(&root_proc.children, this->children.next);
        _detach_from_list(&this->children);
        release_sched_lock();
        for(int i = 0; i < times; i++){
            post_sem(&root_proc.childexit);
        }
        acquire_sched_lock();
    }
    free_pgdir(&this->pgdir);
    release_sched_lock();
    post_sem(&thisproc()->parent->childexit);
    acquire_sched_lock();
    release_spinlock(&plock);
    sched(ZOMBIE);
    PANIC(); // prevent the warning of 'no_return function returns'
}

Proc* find_proc(int pid, Proc* current){
    if(current->pid == pid && !is_unused(current)){
        return current;
    }
    _for_in_list(p, &current->children){
        if(p == &current->children){
            continue;
        }
        auto child = container_of(p, Proc, ptnode);
        Proc* temp = find_proc(pid, child);
        if(temp != NULL){
            return temp;
        }
    }
    return NULL;
}

int kill(int pid)
{
    // TODO:
    // Set the killed flag of the proc to true and return 0.
    // Return -1 if the pid is invalid (proc not found).
    acquire_spinlock(&plock);
    Proc* tokill = find_proc(pid, &root_proc);
    release_spinlock(&plock);
    if(tokill != NULL && ((tokill->ucontext->elr) >> 48) == 0){
        tokill->killed = 1;
        alert_proc(tokill);
        return 0;
    }
    return -1;
}