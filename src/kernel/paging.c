#include <aarch64/mmu.h>
#include <common/defines.h>
#include <common/list.h>
#include <common/sem.h>
#include <common/string.h>
#include <fs/block_device.h>
#include <fs/cache.h>
#include <kernel/mem.h>
#include <kernel/paging.h>
#include <kernel/printk.h>
#include <kernel/proc.h>
#include <kernel/pt.h>
#include <kernel/sched.h>

#define MASK (-(1 << 12))
#define CLEAN(addr) (addr & MASK)

void init_sections(ListNode *section_head) {
    /* (Final) TODO BEGIN */
    auto section_ptr = (struct section *)kalloc(sizeof(struct section));
    _insert_into_list(section_head, &section_ptr->stnode);
    init_sleeplock(&section_ptr->sleepLock);
    section_ptr->begin = 0;
    section_ptr->end = 0;
    section_ptr->flags = (0 | ST_HEAP);
    /* (Final) TODO END */
}

void free_sections(struct pgdir *pd) {
    /* (Final) TODO BEGIN */
    
    /* (Final) TODO END */
}

u64 sbrk(i64 size) {
    /**
     * (Final) TODO BEGIN 
     * 
     * Increase the heap size of current process by `size`.
     * If `size` is negative, decrease heap size. `size` must
     * be a multiple of PAGE_SIZE.
     * 
     * Return the previous heap_end.
     */
    Proc* p = thisproc();
    struct pgdir* pd = &p->pgdir;
    auto sec = container_of(pd->section_head.next, struct section, stnode);
    u64 ans = sec->end;
    sec->end += size * PAGE_SIZE;
    if(size < 0){
        for(i64 i = 0; i < -size; i++){
            auto pte = get_pte(pd, sec->end + i * PAGE_SIZE, false);
            if(pte && (*pte)){
                kfree_page((void*)(P2K(CLEAN(*pte))));
                *pte = NULL;
            }
        }
    }
    attach_pgdir(pd);
    arch_tlbi_vmalle1is();
    return ans;
    /* (Final) TODO END */
}

void* alloc_page_for_user(){
    while(left_page_cnt() <= 1024) {
        return NULL;
    }
    return kalloc_page();
}

void swapin(struct pgdir *pd, struct section *sec) {
    ASSERT(sec->flags & ST_SWAP);
    unalertable_wait_sem(&sec->sleepLock);
    u64 begin = sec->begin;
    u64 end = sec->end;
    for(u64 i = begin; i < end; i += PAGE_SIZE){
        auto pte = get_pte(pd, i, 0);
        if(pte && (*pte)) {
            u32 bno = (*pte);
            void* newpage = alloc_page_for_user();
            read_page_from_disk(newpage,(u32)bno);
            // for(int i = 0; i < 8; i++){
            //     block_device.read((u32)bno + i, (u8*)newpage + i * BLOCK_SIZE);
            // }
            *pte = K2P(newpage) | PTE_USER_DATA;
            release_8_blocks(bno);
        }
    }
    attach_pgdir(pd);
    arch_tlbi_vmalle1is();
    sec->flags &= ~ST_SWAP;
    post_sem(&sec->sleepLock);
}

int pgfault_handler(u64 iss) {
    Proc *p = thisproc();
    struct pgdir *pd = &p->pgdir;
    u64 addr =
            arch_get_far(); // Attempting to access this address caused the page fault

    /** 
     * (Final) TODO BEGIN
     * 
     * 1. Find the section struct which contains the faulting address `addr`.
     * 2. Check section flags to determine page fault type.
     * 3. Handle the page fault accordingly.
     * 4. Return to user code or kill the process.
     */
    struct section* sec = NULL;
    _for_in_list(p, &pd->section_head){
        if(p == &pd->section_head) {
            continue;
        }
        sec = container_of(p, struct section, stnode);
        if(addr >= sec->begin){
            break;
        }
    }
    ASSERT(sec);
    auto pte = get_pte(pd, addr, 1);
    if(*pte == NULL) {
        if(sec->flags & ST_SWAP) {
            swapin(pd, sec);
        }else{
            *pte = K2P(alloc_page_for_user()) | PTE_USER_DATA;
        }
    }else if(*pte & PTE_RO){
        auto p = alloc_page_for_user();
        kfree_page((void*)P2K(PTE_ADDRESS(*pte)));
        memmove(p, (void*)P2K(PTE_ADDRESS(*pte)), PAGE_SIZE);
        ASSERT(p != NULL);
        // ASSERT(check_zero_page());
        *pte = K2P(p) | PTE_USER_DATA;
    }else if(!(*pte & PTE_VALID) && (sec->flags & ST_SWAP)){
        swapin(pd, sec);
    }
    attach_pgdir(pd);
    arch_tlbi_vmalle1is();
    return iss;
    /* (Final) TODO END */
}

void copy_sections(ListNode *from_head, ListNode *to_head)
{
    /* (Final) TODO BEGIN */

    /* (Final) TODO END */
}
