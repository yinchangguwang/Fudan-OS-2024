#include <aarch64/mmu.h>
#include <common/rc.h>
#include <common/spinlock.h>
#include <driver/memlayout.h>
#include <kernel/mem.h>
#include <kernel/printk.h>
#include <common/list.h>
#include <common/string.h>

RefCount kalloc_page_cnt;
SpinLock memlock;
RefCount refpage[PHYSTOP / PAGE_SIZE];
static QueueNode* pages;
void* zero_page = 0;
extern char end[];

typedef struct node
{
    struct node* next;
    u16 size;
    u16 used;
    bool free;
}node;
node* free[4];

void kinit() {
    init_rc(&kalloc_page_cnt);
    init_spinlock(&memlock);
    for(u64 p = PAGE_BASE((u64)&end) + 2 * PAGE_SIZE; p < P2K(PHYSTOP); p += PAGE_SIZE){
        add_to_queue(&pages, (QueueNode*)p);
    }
    zero_page = (void*)(PAGE_BASE((u64)&end) + PAGE_SIZE);
    memset(zero_page, 0, PAGE_SIZE);
    increment_rc(&refpage[K2P(zero_page) / PAGE_SIZE]);
}

void* kalloc_page() {
    increment_rc(&kalloc_page_cnt);
    void* page = fetch_from_queue(&pages);
    increment_rc(&refpage[K2P(page) / PAGE_SIZE]);
    return page;
}

void kfree_page(void* p) {
    decrement_rc(&refpage[K2P(p) / PAGE_SIZE]);
    if(refpage[K2P(p) / PAGE_SIZE].count == 0){
        decrement_rc(&kalloc_page_cnt);
        add_to_queue(&pages, (QueueNode*)p);
    }
    return;
}

void merge(node *h){
    node*p = h->next;
    while(p != NULL && p->free == 1 && PAGE_BASE((u64)h) == PAGE_BASE((u64)p)){
        h->next = p->next;
        h->size += p->size + sizeof(node);
        p = p->next;
    }
    // node*p = h->next;
    // if(p == NULL) return;
    // if(p->free == 1 && PAGE_BASE((u64)h) == PAGE_BASE((u64)p)){
    //     h->next = p->next;
    //     h->size += p->size + sizeof(node);
    // }
}

void* kalloc(unsigned long long size) {
    acquire_spinlock(&memlock);
    int alignment;
    if(size % 8 == 0){
        alignment = 8;
    }
    else{
        alignment = 4;
    }
    // else if(size % 4 == 0){
    //     alignment = 4;
    // }
    // else{
    //     alignment = 2;
    // }
    // printk("align:%d\n", alignment);
    node* h = free[cpuid()];
    while(h){
        if(h->free == 0){
            // if the block too large, split
            u64 temp = (u64)h + sizeof(node) + h->used;
            temp = (temp / alignment + (temp % alignment != 0)) * alignment;
            if(h->size >= (temp - (u64)h - sizeof(node)) + (u16)sizeof(node)){
                // printk("split\n");
                node* p = (node*)temp;
                p->size = h->size - (temp - (u64)h - sizeof(node)) - sizeof(node);
                p->free = 1;
                p->next = h->next;
                h->next = p;
                h->size = (temp - (u64)h - sizeof(node));
            }
        }
        else if(h->free == 1){
            // if next is also free, merge
            merge(h);
            // if find suitable block
            u64 temp = (u64)h + sizeof(node);
            temp = (temp / alignment + (temp % alignment != 0)) * alignment;
            if(((u64)h % alignment == 0) && (h->size >= (u16)size)){
                // printk("find\n");
                break;
            }
            else if(((u64)h % alignment != 0) && (h->size >= (u16)size + (temp - (u64)h - sizeof(node)) + (u16)sizeof(node))){
                // printk("split\n");
                node* p = (node*)temp;
                p->size = h->size - (temp - (u64)h - sizeof(node)) - sizeof(node);
                p->free = 1;
                p->next = h->next;
                h->next = p;
                h->size = (temp - (u64)h - sizeof(node));
                h = p;
                break;
            }
        }
        h = h->next;
    }
    // if no suitable block, alloc a new page and insert to free[] head
    if(h == NULL){
        node* p = (node*)kalloc_page();
        // printk("new page\n");
        p->next = free[cpuid()];
        p->size = PAGE_SIZE - sizeof(node);
        p->free = 1;
        free[cpuid()] = p;
        h = p;
    }
    h->used = size;
    h->free = 0;
    release_spinlock(&memlock);
    return (void*)((u64)h + sizeof(node));
}

void kfree(void* ptr) {
    acquire_spinlock(&memlock);
    node* h = (node*)((u64)ptr - sizeof(node));
    // if next is also free, merge
    merge(h);
    h->free = 1;
    // printk("free:%d", h->size);
    release_spinlock(&memlock);
}
