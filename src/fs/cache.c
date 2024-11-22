#include <common/bitmap.h>
#include <common/string.h>
#include <fs/cache.h>
#include <kernel/mem.h>
#include <kernel/printk.h>
#include <kernel/proc.h>

/**
    @brief the private reference to the super block.

    @note we need these two variables because we allow the caller to
            specify the block device and super block to use.
            Correspondingly, you should NEVER use global instance of
            them, e.g. `get_super_block`, `block_device`

    @see init_bcache
 */
static const SuperBlock *sblock;

/**
    @brief the reference to the underlying block device.
 */
static const BlockDevice *device; 

/**
    @brief global lock for block cache.

    Use it to protect anything you need.

    e.g. the list of allocated blocks, etc.
 */
static SpinLock lock;

/**
    @brief the list of all allocated in-memory block.

    We use a linked list to manage all allocated cached blocks.

    You can implement your own data structure if you like better performance.

    @see Block
 */
static ListNode head;

static LogHeader header; // in-memory copy of log header block.

/**
    @brief a struct to maintain other logging states.
    
    You may wonder where we store some states, e.g.
    
    * how many atomic operations are running?
    * are we checkpointing?
    * how to notify `end_op` that a checkpoint is done?

    Put them here!

    @see cache_begin_op, cache_end_op, cache_sync
 */
struct {
    /* your fields here */
    SpinLock lock;
    Semaphore sem;
    bool iscommit;
    usize outstanding;
} log;

// read the content from disk.
static INLINE void device_read(Block *block) {
    device->read(block->block_no, block->data);
}

// write the content back to disk.
static INLINE void device_write(Block *block) {
    device->write(block->block_no, block->data);
}

// read log header from disk.
static INLINE void read_header() {
    device->read(sblock->log_start, (u8 *)&header);
}

// write log header back to disk.
static INLINE void write_header() {
    device->write(sblock->log_start, (u8 *)&header);
}

// initialize a block struct.
static void init_block(Block *block) {
    block->block_no = 0;
    init_list_node(&block->node);
    block->acquired = false;
    block->pinned = false;

    init_sleeplock(&block->lock);
    block->valid = false;
    memset(block->data, 0, sizeof(block->data));
}

static usize block_num; // the number of blocks in cache
static SpinLock bitmap_lock;

// see `cache.h`.
static usize get_num_cached_blocks() {
    // TODO
    return block_num;
}

// see `cache.h`.
static Block *cache_acquire(usize block_no) {
    // TODO
    acquire_spinlock(&lock);
    Block* blk = NULL;
    _for_in_list(p, &head){
        if(p == &head){
            continue;
        }
        Block* curr = container_of(p, Block, node);
        if(curr->block_no == block_no){
            blk = curr;
            break;
        }
    }
    // if acquired block is in cache
    if(blk){
        blk->acquired = 1;
        release_spinlock(&lock);
        if(!wait_sem(&blk->lock)){
            PANIC();
        }
        acquire_spinlock(&lock);
        _detach_from_list(&blk->node);
        _insert_into_list(&head, &blk->node);
        release_spinlock(&lock);
        return blk;
    }
    // if acquired block is not in cache
    // if the number of cached blocks is no less than this threshold
    if(block_num >= EVICTION_THRESHOLD){
        ListNode* p = head.prev;
        while(true){
            if(p == &head || block_num < EVICTION_THRESHOLD){
                break;
            }
            ListNode* q = p->prev;
            Block* curr = container_of(p, Block, node);
            if(!curr->acquired && !curr->pinned){
                block_num--;
                _detach_from_list(&curr->node);
                kfree(curr);
            }
            p = q;
        }
    }
    blk = kalloc(sizeof(Block));
    init_block(blk);
    if(!wait_sem(&blk->lock)){
        PANIC();
    }
    block_num++;
    blk->acquired = 1;
    blk->block_no = block_no;
    blk->valid = 1; // the content of block loaded from disk
    release_spinlock(&lock);
    device_read(blk);
    acquire_spinlock(&lock);
    _insert_into_list(&head, &blk->node);
    release_spinlock(&lock);
    return blk;
}

// see `cache.h`.
static void cache_release(Block *block) {
    // TODO
    acquire_spinlock(&lock);
    block->acquired = 0;
    post_sem(&block->lock);
    release_spinlock(&lock);
}

// see `cache.h`.
static void cache_begin_op(OpContext *ctx) {
    // TODO
    acquire_spinlock(&log.lock);
    ctx->rm = 0;
    while(log.iscommit || (header.num_blocks + (log.outstanding + 1) * OP_MAX_NUM_BLOCKS) > LOG_MAX_SIZE){
        release_spinlock(&log.lock);
        if(!wait_sem(&log.sem)){
            PANIC();
        }
        acquire_spinlock(&log.lock);
    }
    log.outstanding++;
    release_spinlock(&log.lock);
}

// see `cache.h`.
static void cache_sync(OpContext *ctx, Block *block) {
    // TODO
    if(ctx == NULL){
        device_write(block);
        return;
    }
    acquire_spinlock(&log.lock);
    block->pinned = 1;
    for(usize i = 0; i < header.num_blocks; i++){
        if(block->block_no == header.block_no[i]){
            release_spinlock(&log.lock);
            return;
        }
    }
    if(ctx->rm >= OP_MAX_NUM_BLOCKS || header.num_blocks >= LOG_MAX_SIZE){
        PANIC();
    }
    header.block_no[header.num_blocks++] = block->block_no;
    ctx->rm++;
    release_spinlock(&log.lock);
}

// see `cache.h`.
static void cache_end_op(OpContext *ctx) {
    // TODO
    acquire_spinlock(&log.lock);
    if(log.iscommit){
        PANIC();
    }
    log.outstanding--;
    if(log.outstanding > 0){
        post_sem(&log.sem);
        release_spinlock(&log.lock);
        return;
    }
    log.iscommit = 1;
    for(usize i = 0; i < header.num_blocks; i++){
        release_spinlock(&log.lock);
        Block* from = cache_acquire(header.block_no[i]);
        Block* to = cache_acquire(sblock->log_start + 1 + i);
        for(usize j = 0; j < BLOCK_SIZE; j++){
            to->data[j] = from->data[j];
        }
        cache_sync(NULL, to);
        cache_release(from);
        cache_release(to);
        acquire_spinlock(&log.lock);
    }
    release_spinlock(&log.lock);
    write_header();
    acquire_spinlock(&log.lock);
    for(usize i = 0; i < header.num_blocks; i++){
        Block* curr = cache_acquire(header.block_no[i]);
        cache_sync(NULL, curr);
        curr->pinned = 0;
        cache_release(curr);
    }
    header.num_blocks = 0;
    release_spinlock(&log.lock);
    write_header();
    acquire_spinlock(&log.lock);
    log.iscommit = 0;
    post_all_sem(&log.sem);
    release_spinlock(&log.lock);
    return (void)ctx;
}

// see `cache.h`.
void init_bcache(const SuperBlock *_sblock, const BlockDevice *_device) {
    sblock = _sblock;
    device = _device;

    // TODO
    init_spinlock(&lock);
    init_spinlock(&log.lock);
    init_spinlock(&bitmap_lock);
    init_list_node(&head);
    block_num = 0;
    header.num_blocks = 0;
    log.iscommit = 0;
    log.outstanding = 0;
    init_sem(&log.sem, 0);
    read_header();
    for(usize i = 0; i < header.num_blocks; i++){
        Block* from = cache_acquire(sblock->log_start + 1 + i);
        Block* to = cache_acquire(header.block_no[i]);
        for(usize j = 0; j < BLOCK_SIZE; j++){
            to->data[j] = from->data[j];
        }
        cache_sync(NULL, to);
        cache_release(from);
        cache_release(to);
    }
    header.num_blocks = 0;
    memset(header.block_no, 0, LOG_MAX_SIZE);
    write_header();
}

// see `cache.h`.
static usize cache_alloc(OpContext *ctx) {
    // TODO
    acquire_spinlock(&bitmap_lock);
    for(usize block_start = 0; block_start < sblock->num_blocks; block_start += BLOCK_SIZE * 8){
        Block* map_blk = cache_acquire(sblock->bitmap_start + (block_start / (BLOCK_SIZE * 8)));
        for(usize map_byte = 0; map_byte < BLOCK_SIZE && block_start + map_byte * 8 < sblock->num_blocks; map_byte++){
            u8 temp = map_blk->data[map_byte];
            for(usize map_bit = 0; map_bit < 8 && block_start + map_byte * 8 + map_bit < sblock->num_blocks; map_bit++){
                // if find a '0' bit, means this block_no available
                if((temp & (1 << map_bit)) == 0){
                    map_blk->data[map_byte] += (1 << map_bit);  // set '1'
                    cache_sync(ctx, map_blk);
                    cache_release(map_blk);
                    Block* blk = cache_acquire(block_start + map_byte * 8 + map_bit);
                    memset(blk->data, 0, BLOCK_SIZE);
                    cache_sync(ctx, blk);
                    cache_release(blk);
                    release_spinlock(&bitmap_lock);
                    return block_start + map_byte * 8 + map_bit;
                }
            }
        }
        cache_release(map_blk);
    }
    release_spinlock(&bitmap_lock);
    PANIC();
}

// see `cache.h`.
static void cache_free(OpContext *ctx, usize block_no) {
    // TODO
    acquire_spinlock(&bitmap_lock);
    // find corresponding bitmap block, byte and bit
    Block* map_blk = cache_acquire(sblock->bitmap_start + block_no / (BLOCK_SIZE * 8));
    usize map_byte = (block_no % (BLOCK_SIZE * 8)) / 8;
    usize map_bit = (block_no % (BLOCK_SIZE * 8)) % 8;
    map_blk->data[map_byte] -= (1 << map_bit);  // set '0'
    cache_sync(ctx, map_blk);
    cache_release(map_blk);
    release_spinlock(&bitmap_lock);
}

BlockCache bcache = {
    .get_num_cached_blocks = get_num_cached_blocks,
    .acquire = cache_acquire,
    .release = cache_release,
    .begin_op = cache_begin_op,
    .sync = cache_sync,
    .end_op = cache_end_op,
    .alloc = cache_alloc,
    .free = cache_free,
};