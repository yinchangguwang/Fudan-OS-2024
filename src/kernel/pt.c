#include <aarch64/intrinsic.h>
#include <common/string.h>
#include <kernel/mem.h>
#include <kernel/pt.h>

PTEntriesPtr get_pte(struct pgdir *pgdir, u64 va, bool alloc)
{
    // TODO:
    // Return a pointer to the PTE (Page Table Entry) for virtual address 'va'
    // If the entry not exists (NEEDN'T BE VALID), allocate it if alloc=true, or return NULL if false.
    // THIS ROUTINUE GETS THE PTE, NOT THE PAGE DESCRIBED BY PTE.
    PTEntriesPtr pt0 = pgdir->pt;
    if(pt0 == NULL){
        if(!alloc){
            return NULL;
        }
        pt0 = kalloc_page();
        memset(pt0, 0, PAGE_SIZE);
        pgdir->pt = pt0;
    }
    if(!(pt0[VA_PART0(va)] & PTE_VALID)){
        if(!alloc){
            return NULL;
        }
        void* new_page = kalloc_page();
        memset(new_page, 0, PAGE_SIZE);
        pt0[VA_PART0(va)] = K2P(new_page) | PTE_TABLE;
    }
    PTEntriesPtr pt1 = (PTEntriesPtr)P2K(PTE_ADDRESS(pt0[VA_PART0(va)]));
    if(!(pt1[VA_PART1(va)] & PTE_VALID)){
        if(!alloc){
            return NULL;
        }
        void* new_page = kalloc_page();
        memset(new_page, 0, PAGE_SIZE);
        pt1[VA_PART1(va)] = K2P(new_page) | PTE_TABLE;
    }
    PTEntriesPtr pt2 = (PTEntriesPtr)P2K(PTE_ADDRESS(pt1[VA_PART1(va)]));
    if(!(pt2[VA_PART2(va)] & PTE_VALID)){
        if(!alloc){
            return NULL;
        }
        void* new_page = kalloc_page();
        memset(new_page, 0, PAGE_SIZE);
        pt2[VA_PART2(va)] = K2P(new_page) | PTE_TABLE;
    }
    PTEntriesPtr pt3 = (PTEntriesPtr)P2K(PTE_ADDRESS(pt2[VA_PART2(va)]));
    return &pt3[VA_PART3(va)];
}

void init_pgdir(struct pgdir *pgdir)
{
    pgdir->pt = NULL;
} 

void free_pt_pages(PTEntriesPtr p, int deep){
    if(p == NULL || deep == 3){
        kfree_page(p);
        return;
    }
    for(int i = 0; i < N_PTE_PER_TABLE; i++){
        if(p[i] != NULL){
            PTEntriesPtr next_level = (PTEntriesPtr)P2K(PTE_ADDRESS(p[i]));
            free_pt_pages(next_level, deep + 1);
            p[i] = NULL;
        }
    }
    kfree_page(p);
}

void free_pgdir(struct pgdir *pgdir)
{
    // TODO:
    // Free pages used by the page table. If pgdir->pt=NULL, do nothing.
    // DONT FREE PAGES DESCRIBED BY THE PAGE TABLE
    if(pgdir->pt == NULL) return;
    free_pt_pages(pgdir->pt, 0);
    pgdir->pt = NULL;
}

void attach_pgdir(struct pgdir *pgdir)
{
    extern PTEntries invalid_pt;
    if (pgdir->pt)
        arch_set_ttbr0(K2P(pgdir->pt));
    else
        arch_set_ttbr0(K2P(&invalid_pt));
}

/**
 * Map virtual address 'va' to the physical address represented by kernel
 * address 'ka' in page directory 'pd', 'flags' is the flags for the page
 * table entry.
 */
void vmmap(struct pgdir *pd, u64 va, void *ka, u64 flags)
{
    /* (Final) TODO BEGIN */

    /* (Final) TODO END */
}

/*
 * Copy len bytes from p to user address va in page table pgdir.
 * Allocate physical pages if required.
 * Useful when pgdir is not the current page table.
 */
int copyout(struct pgdir *pd, void *va, void *p, usize len)
{
    /* (Final) TODO BEGIN */

    /* (Final) TODO END */
}