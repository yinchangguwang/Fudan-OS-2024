#include <kernel/mem.h>
#include <kernel/sched.h>
#include <fs/pipe.h>
#include <common/string.h>
#include <kernel/printk.h>

void init_pipe(Pipe *pi)
{
    /* (Final) TODO BEGIN */
    pi->readopen = 1;
    pi->writeopen = 1;
    pi->nread = 0;
    pi->nwrite = 0;
    init_spinlock(&pi->lock);
    init_sem(&pi->rlock, 0);
    init_sem(&pi->wlock, 0);
    /* (Final) TODO END */
}

void init_read_pipe(File *readp, Pipe *pipe)
{
    /* (Final) TODO BEGIN */
    readp->type = FD_PIPE;
    readp->pipe = pipe;
    readp->readable = 1;
    readp->writable = 0;
    /* (Final) TODO END */
}

void init_write_pipe(File *writep, Pipe *pipe)
{
    /* (Final) TODO BEGIN */
    writep->type = FD_PIPE;
    writep->pipe = pipe;
    writep->readable = 0;
    writep->writable = 1;
    /* (Final) TODO END */
}

int pipe_alloc(File **f0, File **f1)
{
    /* (Final) TODO BEGIN */
    Pipe* p = NULL;
    *f0 = *f1 = NULL;
    if((*f0 = file_alloc()) == NULL || (*f1 = file_alloc()) == NULL) {
        goto bad;
    }
    if((p = (Pipe*)kalloc(sizeof(Pipe))) == NULL) {
        goto bad;
    }
    init_pipe(p);
    init_read_pipe(*f0, p);
    init_write_pipe(*f1, p);
    return 0;
bad:
    if(p) {
        kfree((char*)p);
    }
    if(*f0) {
        file_close(*f0);
    }
    if(*f1) {
        file_close(*f1);
    }
    return -1;
    /* (Final) TODO END */
}

void pipe_close(Pipe *pi, int writable)
{
    /* (Final) TODO BEGIN */
    acquire_spinlock(&pi->lock);
    if(writable) {
        pi->writeopen = 0;
        post_sem(&pi->rlock);
    }else {
        pi->readopen = 0;
        post_sem(&pi->wlock);
    }
    if(!pi->readopen && !pi->writeopen) {
        release_spinlock(&pi->lock);
        kfree((void*)pi);
    }else {
        release_spinlock(&pi->lock);
    }
    /* (Final) TODO END */
}

int pipe_write(Pipe *pi, u64 addr, int n)
{
    /* (Final) TODO BEGIN */
    acquire_spinlock(&pi->lock);
    for(int i = 0; i < n; i++) {
        while(pi->nwrite == pi->nread + PIPE_SIZE){
            if(!pi->readopen || thisproc()->killed) {
                release_spinlock(&pi->lock);
                return -1;
            }
            post_sem(&pi->rlock);
            release_spinlock(&pi->lock);
            unalertable_wait_sem(&pi->wlock);
            // acquire_spinlock(&pi->lock);
        }
        pi->data[pi->nwrite % PIPE_SIZE] = *((char*)addr + i);
        pi->nwrite++;
    }
    post_sem(&pi->rlock);
    release_spinlock(&pi->lock);
    return n;
    /* (Final) TODO END */
}

int pipe_read(Pipe *pi, u64 addr, int n)
{
    /* (Final) TODO BEGIN */
    acquire_spinlock(&pi->lock);
    while(pi->nread == pi->nwrite && pi->writeopen) {
        if(thisproc()->killed) {
            release_spinlock(&pi->lock);
            return -1;
        }
        release_spinlock(&pi->lock);
        unalertable_wait_sem(&pi->rlock);
        // acquire_spinlock(&pi->lock);
    }
    int i;
    for(i = 0; i < n; i++){
        if(pi->nread == pi->nwrite) {
            break;
        }
        *((char*)addr + i) = pi->data[pi->nread % PIPE_SIZE];
        pi->nread++;
    }
    post_sem(&pi->wlock);
    release_spinlock(&pi->lock);
    return i;
    /* (Final) TODO END */
}