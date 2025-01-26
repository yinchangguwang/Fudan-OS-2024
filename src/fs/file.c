#include "file.h"
#include <common/defines.h>
#include <common/spinlock.h>
#include <common/sem.h>
#include <fs/inode.h>
#include <common/list.h>
#include <kernel/mem.h>

#include <fs/pipe.h>
#include <fs/fs.h>
#include <fs/cache.h>
#include <kernel/printk.h>

// the global file table.
static struct ftable ftable;

void init_ftable() {
    // TODO: initialize your ftable.
    // printk("in init_ftable\n");
    init_spinlock(&ftable.lock);
}

void init_oftable(struct oftable *oftable) {
    // TODO: initialize your oftable for a new process.
    // printk("in init_oftable\n");
    for(int i = 0; i < 16; i++) {
        oftable->openfile[i] = NULL;
    }
}

/* Allocate a file structure. */
struct file* file_alloc() {
    /* (Final) TODO BEGIN */
    // printk("in file_alloc\n");
    acquire_spinlock(&ftable.lock);
    for(int i = 0; i < NFILE; i++) {
        if(ftable.filelist[i].ref == 0) {
        // if(ftable.filelist[i].ref == 0 && ftable.filelist[i].type == FD_NONE) {
            ftable.filelist[i].ref = 1;
            release_spinlock(&ftable.lock);
            return &(ftable.filelist[i]);
        }
    }
    release_spinlock(&ftable.lock);
    return NULL;
    /* (Final) TODO END */
}

/* Increment ref count for file f. */
struct file* file_dup(struct file* f) {
    /* (Final) TODO BEGIN */
    // printk("in file_dup\n");
    acquire_spinlock(&ftable.lock);
    ASSERT(f->ref >= 1);
    f->ref++;
    release_spinlock(&ftable.lock);
    /* (Final) TODO END */
    return f;
}

/* Close file f. (Decrement ref count, close when reaches 0.) */
void file_close(struct file* f) {
    /* (Final) TODO BEGIN */
    // printk("in file_close\n");
    acquire_spinlock(&ftable.lock);
    ASSERT(f->ref >= 1);
    f->ref--;
    if(f->ref > 0) {
        release_spinlock(&ftable.lock);
        return;
    }
    struct file now = *f;
    f->type = FD_NONE;
    release_spinlock(&ftable.lock);
    if(now.type == FD_PIPE) {
        pipe_close(now.pipe, now.writable);
    }else if(now.type == FD_INODE) {
        OpContext ctx;
        bcache.begin_op(&ctx);
        inodes.put(&ctx, now.ip);
        bcache.end_op(&ctx);
    }
    /* (Final) TODO END */
}

/* Get metadata about file f. */
int file_stat(struct file* f, struct stat* st) {
    /* (Final) TODO BEGIN */
    // printk("in file_stat\n");
    if(f->type == FD_INODE) {
        inodes.lock(f->ip);
        stati(f->ip, st);
        inodes.unlock(f->ip);
        return 0;
    }
    /* (Final) TODO END */
    return -1;
}

/* Read from file f. */
isize file_read(struct file* f, char* addr, isize n) {
    /* (Final) TODO BEGIN */
    // printk("in file_read\n");
    // if(f->readable == 0 || f->type == FD_NONE) {
    if(f->readable == 0) {
        return -1;
    }
    if(f->type == FD_INODE) {
        isize r = 0;
        inodes.lock(f->ip);
        r = inodes.read(f->ip, (u8*)addr, f->off, n);
        if(r > 0){
            f->off += r;
        }
        inodes.unlock(f->ip);
        return r;
    }
    if(f->type == FD_PIPE) {
        return pipe_read(f->pipe, (u64)addr, n);
    }
    PANIC();
    /* (Final) TODO END */
    return 0;
}

/* Write to file f. */
isize file_write(struct file* f, char* addr, isize n) {
    /* (Final) TODO BEGIN */
    // printk("in file_write\n");
    // if(f->writable == 0 || f->type == FD_NONE || n < 0) {
    if(f->writable == 0) {
        return -1;
    }
    if(f->type == FD_INODE) {
        isize maxbytes = ((OP_MAX_NUM_BLOCKS - 4) / 2) * BLOCK_SIZE;
        isize idx = 0;
        while(idx < n) {
            isize len = MIN(n - idx, maxbytes);
            OpContext ctx;
            bcache.begin_op(&ctx);
            inodes.lock(f->ip);
            isize real_len = inodes.write(&ctx, f->ip, (u8*)(addr + idx), f->off, len);
            if(real_len > 0) {
                f->off += real_len;
            }
            inodes.unlock(f->ip);
            bcache.end_op(&ctx);
            if(real_len < 0) {
                break;
            }
            ASSERT(real_len == len);
            idx += real_len;
        }
        if(idx == n) {
            return n;
        }
        return -1;
    }
    if(f->type == FD_PIPE) {
        return pipe_write(f->pipe, (u64)addr, n);
    }
    /* (Final) TODO END */
    return 0;
}