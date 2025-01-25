#include <elf.h>
#include <common/string.h>
#include <common/defines.h>
#include <kernel/console.h>
#include <kernel/proc.h>
#include <kernel/sched.h>
#include <kernel/syscall.h>
#include <kernel/pt.h>
#include <kernel/mem.h>
#include <kernel/paging.h>
#include <aarch64/trap.h>
#include <fs/file.h>
#include <fs/inode.h>
#include <kernel/printk.h>

extern int fdalloc(struct file *f);

int allocuvm(struct pgdir *pgdir, u64 base, u64 stksz, u64 oldsz, u64 newsz){
    ASSERT(stksz % PAGE_SIZE == 0);
    base = base;
    for(u64 a = (oldsz + PAGE_SIZE - 1) / PAGE_SIZE * PAGE_SIZE; a < newsz; a += PAGE_SIZE){
        void* p = kalloc_page();
        ASSERT(p != NULL);
        *get_pte(pgdir, a, true) = K2P(p) | PTE_USER_DATA;
    }
    return newsz;
}


int execve(const char *path, char *const argv[], char *const envp[])
{
    /* (Final) TODO BEGIN */
    // printk("enter execve: path: %s\n", path);
    // printk("path: %s\n", path);
    Proc* p = thisproc();
    struct pgdir old_pgdir = p->pgdir;
    struct pgdir* pgdir = kalloc(sizeof(struct pgdir));
    init_pgdir(pgdir);
    Inode* ip = NULL;
    if(pgdir == NULL) {
        goto bad;
    }
    OpContext ctx;
    bcache.begin_op(&ctx);
    ip = namei(path, &ctx);
    // printk("namei done\n");
    if(ip == NULL) {
        // printk("ip is null\n");
        bcache.end_op(&ctx);
        goto bad;
    }
    inodes.lock(ip);
    Elf64_Ehdr elf;
    if(inodes.read(ip, (u8*)&elf, 0, sizeof(elf)) != sizeof(elf)) {
        goto bad;
    }
    if(!(elf.e_ident[EI_MAG0] == ELFMAG0 && elf.e_ident[EI_MAG1] == ELFMAG1 &&
         elf.e_ident[EI_MAG2] == ELFMAG2 && elf.e_ident[EI_MAG3] == ELFMAG3)) {
        goto bad;
    }
    if(elf.e_ident[EI_CLASS] != ELFCLASS64){
        goto bad;
    }
    int i = 0;
    uint64_t off;
    Elf64_Phdr ph;
    p->pgdir = *pgdir;
    u64 sz = 0, base = 0, stksz = 0;
    int first = 1;
    for(i = 0, off = elf.e_phoff; i < elf.e_phnum; i++, off += sizeof(ph)) {
        if(inodes.read(ip, (u8*)&ph, off, sizeof(ph)) != sizeof(ph)) {
            PANIC();
        }
        if(ph.p_type != PT_LOAD) {
            continue;
        }
        if(ph.p_memsz < ph.p_filesz) {
            PANIC();
        }
        if(ph.p_vaddr + ph.p_memsz < ph.p_vaddr) {
            PANIC();
        }
        if(first) {
            sz = base = ph.p_vaddr;
            first = 0;
            if(base % PAGE_SIZE != 0) {
                PANIC();
            }
        }
        sz = allocuvm(pgdir, base, stksz, sz, ph.p_vaddr + ph.p_memsz);
        if(sz == 0) {
            PANIC();
        }
        attach_pgdir(pgdir);
        arch_tlbi_vmalle1is();
        // printk("OK1\n");
        if(inodes.read(ip, (u8*)ph.p_vaddr, ph.p_offset, ph.p_filesz) != ph.p_filesz) {
            PANIC();
        }
        memset((void*)ph.p_vaddr + ph.p_filesz, 0, ph.p_memsz - ph.p_filesz);
    }
    inodes.unlock(ip);
    inodes.put(&ctx, ip);
    bcache.end_op(&ctx);
    ip = NULL;
    attach_pgdir(&old_pgdir);
    arch_tlbi_vmalle1is();
    char* sp = (char*)USERTOP;
    int argc = 0;
    int envc = 0;
    if(argv){
        for(; argc < MAXARG && argv[argc]; argc++){
            usize len = strlen(argv[argc]) + 1;
            sp -= len;
            copyout(pgdir, sp, argv[argc], len);
        }
    }
    if(envp){
        for(; envc < MAXARG && envp[envc]; envc++){
            usize len = strlen(envp[envc]) + 1;
            sp -= len;
            copyout(pgdir, sp, envp[envc], len);
        }
    }
    void* newsp = (void*)(((usize)sp - (envc + argc + 4) * 8) / 16 * 16);
    copyout(pgdir, newsp, NULL, (void*)sp - newsp);
    attach_pgdir(pgdir);
    arch_tlbi_vmalle1is();
    uint64_t* newargv = newsp + 8;
    uint64_t* newenvp = (void*)newargv + (argc + 1) * 8;
    for(int i = envc - 1; i >= 0; i--){
        newenvp[i] = (uint64_t)sp;
        for(; *sp; sp++);
        sp++;
    }
    for(int i = argc - 1; i >= 0; i--){
        newargv[i] = (uint64_t)sp;
        for(; *sp; sp++);
        sp++;
    }
    *(usize*)(newsp) = argc;
    sp = newsp;
    stksz = (USERTOP - (usize)sp + 10 * PAGE_SIZE - 1) / (10 * PAGE_SIZE) * (10 * PAGE_SIZE);
    copyout(pgdir, (void*)(USERTOP - stksz), 0, stksz - (USERTOP - (usize)sp));
    ASSERT((uint64_t)sp > USERTOP - stksz);
    p->ucontext->sp = (uint64_t)sp;
    p->ucontext->elr = elf.e_entry;
    p->pgdir = *pgdir;
    attach_pgdir(&p->pgdir);
    arch_tlbi_vmalle1is();
    free_pgdir(&old_pgdir);
    // printk("exit execve\n");
    return 0;

bad:
    // printk("bad\n");
    if(pgdir) {
        free_pgdir(pgdir);
    }
    if(ip) {
        inodes.unlock(ip);
        inodes.put(&ctx, ip);
        bcache.end_op(&ctx);
    }
    thisproc()->pgdir = old_pgdir;
    return -1;
    /* (Final) TODO END */
}
