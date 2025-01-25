//
// File-system system calls implementation.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysmacros.h>
#include <sys/mman.h>
#include <stddef.h>

#include "syscall.h"
#include <aarch64/mmu.h>
#include <common/defines.h>
#include <common/spinlock.h>
#include <common/string.h>
#include <fs/file.h>
#include <fs/fs.h>
#include <fs/inode.h>
#include <fs/pipe.h>
#include <kernel/mem.h>
#include <kernel/paging.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/sched.h>

struct iovec {
    void *iov_base; /* Starting address. */
    usize iov_len; /* Number of bytes to transfer. */
};

/** 
 * Get the file object by fd. Return null if the fd is invalid.
 */
static struct file *fd2file(int fd)
{
    /* (Final) TODO BEGIN */
    // printk("in fd2file\n");
    if(fd < 0 || fd >= 16) {
        return NULL;
    }
    return (thisproc()->oftable.openfile[fd]);
    /* (Final) TODO END */
}

/*
 * Allocate a file descriptor for the given file.
 * Takes over file reference from caller on success.
 */
int fdalloc(struct file *f)
{
    /* (Final) TODO BEGIN */
    // printk("in fdalloc\n");
    Proc* p = thisproc();
    for(int i = 0; i < 16; i++) {
        if(p->oftable.openfile[i] == 0) {
            p->oftable.openfile[i] = f;
            return i;
        }
    }
    /* (Final) TODO END */
    return -1;
}

define_syscall(ioctl, int fd, u64 request)
{
    // 0x5413 is TIOCGWINSZ (I/O Control to Get the WINdow SIZe, a magic request
    // to get the stdin terminal size) in our implementation. Just ignore it.
    // printk("in syscall ioctl\n");
    ASSERT(request == 0x5413);
    (void)fd;
    return 0;
}

void get_free_vm(struct pgdir* pd, u64 length, u64* begin, u64* end){
    // get vm area between heap and userstack section
    // a file must auto-mmapped at the top of the area
    *end = (u64)-1;
    _for_in_list(p, &pd->section_head){
        if(p != &pd->section_head){
            auto st = container_of(p, struct section, stnode);
            if(st->flags == ST_HEAP)*begin = st->end;
            if(st->flags == (1 << 6) 
                || st->flags == (1 << 5)
                || st->flags == (1 << 4))*end = MIN(*end, st->begin);
        }
    }
    if(*end - *begin < length){
        // to fix
        *begin = *end = 0;
    }
    else *begin = *end - length;
}

define_syscall(mmap, void *addr, int length, int prot, int flags, int fd,
               int offset)
{
    /* (Final) TODO BEGIN */
    // printk("in syscall mmap\n");
    return 0;
    // if(prot == PROT_NONE || prot&PROT_EXEC || fd < 0 || fd >= 16 || length <= 0)return -1;
    // auto st = (struct section*)kalloc(sizeof(struct section));
    // memset(st, 0, sizeof(struct section));
    // st->flags = flags == MAP_SHARED ? (1 << 5) : (1 << 6);

    // auto this = thisproc();
    // auto f = fd2file(fd);
    // if(!f){
    //     kfree(st);
    //     return -1;
    // }

    // if((prot & PROT_WRITE) && !f->writable && flags != MAP_PRIVATE){
    //     kfree(st);
    //     return -1;
    // }

    // st->fp = file_dup(f);

    // acquire_spinlock(&this->pgdir.lock);
    // if(addr == 0){
    //     u64 free_begin, free_end;
    //     get_free_vm(&this->pgdir, length, &free_begin, &free_end);
    //     if(free_end == free_begin){
    //         // can not find an area
    //         kfree(st);
    //         release_spinlock(&this->pgdir.lock);
    //         return -1;
    //     }
    //     st->end = free_end;
    //     st->begin = st->end - (u64)length;
    // }
    // else{
    //     _for_in_list(p, &this->pgdir.section_head){
    //         if(p != &this->pgdir.section_head){
    //             auto sec = container_of(p, struct section, stnode);
    //             if(sec->begin < (u64)addr + (u64)length && (u64)addr < sec->end){
    //                 kfree(st);
    //                 release_spinlock(&this->pgdir.lock);
    //                 return -1;
    //             }
    //         }
    //     }
    //     st->begin = (u64)addr;
    //     st->end = st->begin + (u64)length;
    // }
    // st->length = (u64)length;
    // st->offset = (int)offset;
    // _insert_into_list(&this->pgdir.section_head, &st->stnode);
    // st->prot = prot;
    // release_spinlock(&this->pgdir.lock);
    // return st->begin;
    /* (Final) TODO END */
}

define_syscall(munmap, void *addr, size_t length)
{
    /* (Final) TODO BEGIN */
    // printk("in syscall munmap\n");
    return (u64)addr + length;
    // auto this = thisproc();
    // acquire_spinlock(&this->pgdir.lock);
    // _for_in_list(p, &this->pgdir.section_head){
    //     if(p != &this->pgdir.section_head){
    //         auto st = container_of(p, struct section, stnode);
    //         if((u64)addr == st->begin){
    //             ASSERT(st->flags == (1 << 6) || st->flags == (1 << 5));
    //             ASSERT(st->fp);
    //             if(length >= st->end - st->begin){
    //                 free_section_pages(&this->pgdir, st);
    //                 _detach_from_list(p);
    //                 file_close(st->fp);
    //                 kfree(st);
    //             }
    //             else {
    //                 auto end = st->begin + length;
    //                 for(auto i = PAGE_BASE(st->begin); i < end; i += PAGE_SIZE){
    //                     auto pte = get_pte(&this->pgdir, i, false);
    //                     if(st->fp->type == FD_INODE && get_page_ref(P2K(PTE_ADDRESS(*pte))) == 1){
    //                         u64 this_begin = MAX(i, st->begin);
    //                         u64 this_end = MIN(i + PAGE_SIZE, end);
    //                         st->fp->off = st->offset + this_begin - st->begin;
    //                         file_write(st->fp, (char*)P2K(PTE_ADDRESS(*pte)), MIN((u64)PAGE_SIZE, this_end - this_begin));
    //                     }
    //                     else if(st->fp->type == FD_PIPE){
    //                         PANIC();
    //                     }
    //                     else PANIC();
    //                     kfree_page((void*)P2K(PTE_ADDRESS(*pte)));
    //                     *pte = 0;
    //                 }
    //                 st->begin = end;
    //             }
    //             break;
    //         }
    //     }
    // }
    // release_spinlock(&this->pgdir.lock);
    // return 0;
    /* (Final) TODO END */
}

define_syscall(dup, int fd)
{
    // printk("in syscall dup\n");
    struct file *f = fd2file(fd);
    if (!f)
        return -1;
    fd = fdalloc(f);
    if (fd < 0)
        return -1;
    file_dup(f);
    return fd;
}

define_syscall(read, int fd, char *buffer, int size)
{
    // printk("in syscall read\n");
    struct file *f = fd2file(fd);
    if (!f || size <= 0 || !user_writeable(buffer, size))
        return -1;
    return file_read(f, buffer, size);
}

define_syscall(write, int fd, char *buffer, int size)
{
    // printk("in syscall write\n");
    struct file *f = fd2file(fd);
    if (!f || size <= 0 || !user_readable(buffer, size))
        return -1;
    return file_write(f, buffer, size);
}

define_syscall(writev, int fd, struct iovec *iov, int iovcnt)
{
    // printk("in syscall writev\n");
    struct file *f = fd2file(fd);
    struct iovec *p;
    if (!f || iovcnt <= 0 || !user_readable(iov, sizeof(struct iovec) * iovcnt))
        return -1;
    usize tot = 0;
    for (p = iov; p < iov + iovcnt; p++) {
        if (!user_readable(p->iov_base, p->iov_len))
            return -1;
        tot += file_write(f, p->iov_base, p->iov_len);
    }
    return tot;
}

define_syscall(close, int fd)
{
    /* (Final) TODO BEGIN */
    File* f = fd2file(fd);
    thisproc()->oftable.openfile[fd] = NULL;
    file_close(f);
    /* (Final) TODO END */
    return 0;
}

define_syscall(fstat, int fd, struct stat *st)
{
    // printk("in syscall fstat\n");
    struct file *f = fd2file(fd);
    if (!f || !user_writeable(st, sizeof(*st)))
        return -1;
    return file_stat(f, st);
}

define_syscall(newfstatat, int dirfd, const char *path, struct stat *st,
               int flags)
{
    // printk("in syscall newfstatat\n");
    if (!user_strlen(path, 256) || !user_writeable(st, sizeof(*st)))
        return -1;
    if (dirfd != AT_FDCWD) {
        printk("sys_fstatat: dirfd unimplemented\n");
        return -1;
    }
    if (flags != 0) {
        printk("sys_fstatat: flags unimplemented\n");
        return -1;
    }

    Inode *ip;
    OpContext ctx;
    bcache.begin_op(&ctx);
    if ((ip = namei(path, &ctx)) == 0) {
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.lock(ip);
    stati(ip, st);
    inodes.unlock(ip);
    inodes.put(&ctx, ip);
    bcache.end_op(&ctx);

    return 0;
}

static int isdirempty(Inode *dp)
{
    // printk("in isdirempty\n");
    usize off;
    DirEntry de;

    for (off = 2 * sizeof(de); off < dp->entry.num_bytes; off += sizeof(de)) {
        if (inodes.read(dp, (u8 *)&de, off, sizeof(de)) != sizeof(de))
            PANIC();
        if (de.inode_no != 0)
            return 0;
    }
    return 1;
}

define_syscall(unlinkat, int fd, const char *path, int flag)
{
    // printk("in syscall unlinkat\n");
    ASSERT(fd == AT_FDCWD && flag == 0);
    Inode *ip, *dp;
    DirEntry de;
    char name[FILE_NAME_MAX_LENGTH];
    usize off;
    if (!user_strlen(path, 256))
        return -1;
    OpContext ctx;
    bcache.begin_op(&ctx);
    if ((dp = nameiparent(path, name, &ctx)) == 0) {
        bcache.end_op(&ctx);
        return -1;
    }

    inodes.lock(dp);

    // Cannot unlink "." or "..".
    if (strncmp(name, ".", FILE_NAME_MAX_LENGTH) == 0 ||
        strncmp(name, "..", FILE_NAME_MAX_LENGTH) == 0)
        goto bad;

    usize inumber = inodes.lookup(dp, name, &off);
    if (inumber == 0)
        goto bad;
    ip = inodes.get(inumber);
    inodes.lock(ip);

    if (ip->entry.num_links < 1)
        PANIC();
    if (ip->entry.type == INODE_DIRECTORY && !isdirempty(ip)) {
        inodes.unlock(ip);
        inodes.put(&ctx, ip);
        goto bad;
    }

    memset(&de, 0, sizeof(de));
    if (inodes.write(&ctx, dp, (u8 *)&de, off, sizeof(de)) != sizeof(de))
        PANIC();
    if (ip->entry.type == INODE_DIRECTORY) {
        dp->entry.num_links--;
        inodes.sync(&ctx, dp, true);
    }
    inodes.unlock(dp);
    inodes.put(&ctx, dp);
    ip->entry.num_links--;
    inodes.sync(&ctx, ip, true);
    inodes.unlock(ip);
    inodes.put(&ctx, ip);
    bcache.end_op(&ctx);
    return 0;

bad:
    inodes.unlock(dp);
    inodes.put(&ctx, dp);
    bcache.end_op(&ctx);
    return -1;
}

/**
    @brief create an inode at `path` with `type`.

    If the inode exists, just return it.

    If `type` is directory, you should also create "." and ".." entries and link
   them with the new inode.

    @note BE careful of handling error! You should clean up ALL the resources
   you allocated and free ALL acquired locks when error occurs. e.g. if you
   allocate a new inode "/my/dir", but failed to create ".", you should free the
   inode "/my/dir" before return.

    @see `nameiparent` will find the parent directory of `path`.

    @return Inode* the created inode, or NULL if failed.
 */
Inode *create(const char *path, short type, short major, short minor,
              OpContext *ctx)
{
    /* (Final) TODO BEGIN */
    // printk("in create\n");
    Inode *ip, *dir;
    char name[FILE_NAME_MAX_LENGTH];
    dir = nameiparent(path, name, ctx);
    if (dir == NULL) {
        return NULL;
    }
    inodes.lock(dir);
    usize inode_no = inodes.lookup(dir, name, NULL);
    // printk("major: %d\n", major);
    // printk("in create: inode_no: %lld\n", inode_no);
    // ip = inodes.get(inodes.lookup(dir, name, NULL));
    // if (ip != NULL) {
    //     inodes.unlock(dir);
    //     inodes.put(ctx, dir);
    //     inodes.lock(ip);
    //     if(type == INODE_REGULAR && ip->entry.type == INODE_REGULAR) {
    //         return ip;
    //     }
    //     inodes.unlock(ip);
    //     inodes.put(ctx, ip);
    //     return NULL;
    // }
    if (inode_no != 0) {
        ip = inodes.get(inode_no);
        inodes.unlock(dir);
        inodes.put(ctx, dir);
        inodes.lock(ip);
        if(type == INODE_REGULAR && ip->entry.type == INODE_REGULAR) {
            return ip;
        }
        inodes.unlock(ip);
        inodes.put(ctx, ip);
        return NULL;
    }
    ip = inodes.get(inodes.alloc(ctx, type));
    ASSERT(ip != NULL);
    inodes.lock(ip);
    ip->entry.major = major;
    ip->entry.minor = minor;
    ip->entry.num_links = 1;
    inodes.sync(ctx, ip, true);
    if (type == INODE_DIRECTORY) {
        dir->entry.num_links++;
        inodes.sync(ctx, dir, true);
        inodes.insert(ctx, ip, ".", ip->inode_no);
        inodes.insert(ctx, ip, "..", dir->inode_no);
    } 
    inodes.insert(ctx, dir, name, ip->inode_no);
    inodes.unlock(dir);
    inodes.put(ctx, dir);
    // printk("create end\n");
    return ip;
    /* (Final) TODO END */
}

define_syscall(openat, int dirfd, const char *path, int omode)
{
    // printk("in syscall openat\n");
    int fd;
    struct file *f;
    Inode *ip;

    if (!user_strlen(path, 256))
        return -1;

    if (dirfd != AT_FDCWD) {
        printk("sys_openat: dirfd unimplemented\n");
        return -1;
    }

    OpContext ctx;
    bcache.begin_op(&ctx);
    if (omode & O_CREAT) {
        // FIXME: Support acl mode.
        ip = create(path, INODE_REGULAR, 0, 0, &ctx);
        if (ip == 0) {
            bcache.end_op(&ctx);
            return -1;
        }
    } else {
        if ((ip = namei(path, &ctx)) == 0) {
            bcache.end_op(&ctx);
            return -1;
        }
        inodes.lock(ip);
    }

    if ((f = file_alloc()) == 0 || (fd = fdalloc(f)) < 0) {
        if (f)
            file_close(f);
        inodes.unlock(ip);
        inodes.put(&ctx, ip);
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.unlock(ip);
    bcache.end_op(&ctx);

    f->type = FD_INODE;
    f->ip = ip;
    f->off = 0;
    f->readable = !(omode & O_WRONLY);
    f->writable = (omode & O_WRONLY) || (omode & O_RDWR);
    return fd;
}

define_syscall(mkdirat, int dirfd, const char *path, int mode)
{
    Inode *ip;
    if (!user_strlen(path, 256))
        return -1;
    if (dirfd != AT_FDCWD) {
        printk("sys_mkdirat: dirfd unimplemented\n");
        return -1;
    }
    if (mode != 0) {
        printk("sys_mkdirat: mode unimplemented\n");
        return -1;
    }
    OpContext ctx;
    bcache.begin_op(&ctx);
    if ((ip = create(path, INODE_DIRECTORY, 0, 0, &ctx)) == 0) {
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.unlock(ip);
    inodes.put(&ctx, ip);
    bcache.end_op(&ctx);
    return 0;
}

define_syscall(mknodat, int dirfd, const char *path, mode_t mode, dev_t dev)
{
    // printk("in syscall mknodat\n");
    // printk("mode: %d\n", mode);
    // printk("dev: %ld\n", dev);
    Inode *ip;
    if (!user_strlen(path, 256))
        return -1;
    if (dirfd != AT_FDCWD) {
        printk("sys_mknodat: dirfd unimplemented\n");
        return -1;
    }

    unsigned int ma = major(dev);
    unsigned int mi = minor(dev);
    printk("mknodat: path '%s', major:minor %u:%u\n", path, ma, mi);
    OpContext ctx;
    bcache.begin_op(&ctx);
    if ((ip = create(path, INODE_DEVICE, (short)ma, (short)mi, &ctx)) == 0) {
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.unlock(ip);
    inodes.put(&ctx, ip);
    bcache.end_op(&ctx);
    return 0;
}

define_syscall(chdir, const char *path)
{
    /**
     * (Final) TODO BEGIN 
     * 
     * Change the cwd (current working dictionary) of current process to 'path'.
     * You may need to do some validations.
     */
    // printk("in syscall chdir\n");
    Proc *p = thisproc();
    OpContext ctx;
    bcache.begin_op(&ctx);
    Inode* ip = namei(path, &ctx);
    if(ip == NULL) {
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.lock(ip);
    if(ip->entry.type != INODE_DIRECTORY) {
        inodes.unlock(ip);
        inodes.put(&ctx, ip);
        bcache.end_op(&ctx);
        return -1;
    }
    inodes.unlock(ip);
    inodes.put(&ctx, p->cwd);
    bcache.end_op(&ctx);
    p->cwd = ip;
    return 0;
    /* (Final) TODO END */
}

define_syscall(pipe2, int pipefd[2], int flags)
{

    /* (Final) TODO BEGIN */
    // printk("in syscall pipe2\n");
    File *rf, *wf;
    if(flags){
        return -1;
    }
    if(pipe_alloc(&rf, &wf) < 0){
        return -1;
    }
    int rfd = fdalloc(rf);
    int wfd = fdalloc(wf);
    if(rfd < 0 || wfd < 0){
        if(rfd >= 0){
            thisproc()->oftable.openfile[rfd] = NULL;
        }
        file_close(rf);
        file_close(wf);
        return -1;
    }
    pipefd[0] = rfd;
    pipefd[1] = wfd;
    return 0;
    /* (Final) TODO END */
}