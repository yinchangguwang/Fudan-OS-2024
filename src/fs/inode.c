#include <common/string.h>
#include <fs/inode.h>
#include <kernel/mem.h>
#include <kernel/printk.h>

#include <sys/stat.h>
#include <kernel/sched.h>
#include <kernel/console.h>

/**
    @brief the private reference to the super block.

    @note we need these two variables because we allow the caller to
            specify the block cache and super block to use.
            Correspondingly, you should NEVER use global instance of
            them.

    @see init_inodes
 */
static const SuperBlock* sblock;

/**
    @brief the reference to the underlying block cache.
 */
static const BlockCache* cache;

/**
    @brief global lock for inode layer.

    Use it to protect anything you need.

    e.g. the list of allocated blocks, ref counts, etc.
 */
static SpinLock lock;

/**
    @brief the list of all allocated in-memory inodes.

    We use a linked list to manage all allocated inodes.

    You can implement your own data structure if you want better performance.

    @see Inode
 */
static ListNode head;


// return which block `inode_no` lives on.
static INLINE usize to_block_no(usize inode_no) {
    return sblock->inode_start + (inode_no / (INODE_PER_BLOCK));
}

// return the pointer to on-disk inode.
static INLINE InodeEntry* get_entry(Block* block, usize inode_no) {
    return ((InodeEntry*)block->data) + (inode_no % INODE_PER_BLOCK);
}

// return address array in indirect block.
static INLINE u32* get_addrs(Block* block) {
    return ((IndirectBlock*)block->data)->addrs;
}

// initialize inode tree.
void init_inodes(const SuperBlock* _sblock, const BlockCache* _cache) {
    init_spinlock(&lock);
    init_list_node(&head);
    sblock = _sblock;
    cache = _cache;

    if (ROOT_INODE_NO < sblock->num_inodes)
        inodes.root = inodes.get(ROOT_INODE_NO);
    else
        printk("(warn) init_inodes: no root inode.\n");
}

// initialize in-memory inode.
static void init_inode(Inode* inode) {
    init_sleeplock(&inode->lock);
    init_rc(&inode->rc);
    init_list_node(&inode->node);
    inode->inode_no = 0;
    inode->valid = false;
}

// see `inode.h`.
static usize inode_alloc(OpContext* ctx, InodeType type) {
    ASSERT(type != INODE_INVALID);

    // TODO
    for(usize i = 1; i < sblock->num_inodes; i++) {
        usize block_no = to_block_no(i);
        Block* block = cache->acquire(block_no);
        InodeEntry* entry = get_entry(block, i);
        if(entry->type == INODE_INVALID) {
            memset(entry, 0, sizeof(InodeEntry));
            entry->type = type;
            cache->sync(ctx, block);
            cache->release(block);
            return i;
        }
        cache->release(block);
    }
    PANIC();
    return 0;
    // usize root_block_idx = to_block_no(ROOT_INODE_NO) - sblock->inode_start;
    // usize inode_no = ROOT_INODE_NO;
    // bool panic_flag = true;
    // for(usize i = root_block_idx; i < sblock->num_inodes * sizeof(InodeEntry) / BLOCK_SIZE && panic_flag; i++){
    //     auto block = cache->acquire(sblock->inode_start + i);
    //     for(usize j = (i != root_block_idx ? 0 : ROOT_INODE_NO + 1); j < BLOCK_SIZE / sizeof(InodeEntry); j++){
    //         auto entry = get_entry(block, j);
    //         inode_no++;
    //         if(entry->type == INODE_INVALID){
    //             memset(entry, 0, sizeof(InodeEntry));
    //             entry->type = type;
    //             cache->sync(ctx, block);
    //             panic_flag = false;
    //             break;
    //         }
    //     }
    //     cache->release(block);
    // }
    // if(panic_flag)PANIC();
    // return inode_no;
}


// static void inode_sync(OpContext* ctx, Inode* inode, bool do_write);


// see `inode.h`.
static void inode_lock(Inode* inode) {
    ASSERT(inode->rc.count > 0);
    // TODO
    unalertable_wait_sem(&inode->lock);

    // if(!wait_sem(&inode->lock)){
    //     inode->valid = false; // if inode->valid is false after calling this function, it means been killed
    //     return;
    // }
    // else if(inode->valid == false){
    //     inode_sync(NULL, inode, false);
    // }
}

// see `inode.h`.
static void inode_unlock(Inode* inode) {
    ASSERT(inode->rc.count > 0);
    // TODO
    post_sem(&inode->lock);
}

// see `inode.h`.
static void inode_sync(OpContext* ctx, Inode* inode, bool do_write) {
    // TODO
    if(inode->valid && do_write) {
        usize block_no = to_block_no(inode->inode_no);
        Block* block = cache->acquire(block_no);
        InodeEntry* entry = get_entry(block, inode->inode_no);
        memcpy(entry, &inode->entry, sizeof(InodeEntry));
        cache->sync(ctx, block);
        cache->release(block);
    }
    else if(!inode->valid) {
        usize block_no = to_block_no(inode->inode_no);
        Block* block = cache->acquire(block_no);
        InodeEntry* entry = get_entry(block, inode->inode_no);
        memcpy(&inode->entry, entry, sizeof(InodeEntry));
        inode->valid = true;
        cache->release(block);
    }
    // if(do_write){
    //     if(inode->valid == false)PANIC();
    //     else{
    //         usize block_no = to_block_no(inode->inode_no);
    //         auto block = cache->acquire(block_no);
    //         memcpy(get_entry(block, inode->inode_no), &inode->entry, sizeof(InodeEntry));
    //         cache->sync(ctx, block);
    //         cache->release(block);
    //     }
    // }
    // else{
    //     if(inode->valid == false){
    //         usize block_no = to_block_no(inode->inode_no);
    //         auto block = cache->acquire(block_no);
    //         memcpy(&inode->entry, get_entry(block, inode->inode_no), sizeof(InodeEntry));
    //         cache->release(block);
    //         inode->valid = true;
    //     }
    // }
}

// see `inode.h`.
static Inode* inode_get(usize inode_no) {
    // printk("enter inode_get\n");
    ASSERT(inode_no > 0);
    // printk("inode_no: %lld\n", inode_no);
    // printk("sblock num inodes: %d\n", sblock->num_inodes);
    ASSERT(inode_no < sblock->num_inodes);
    acquire_spinlock(&lock);
    // TODO
    _for_in_list(p, &head) {
        if(p == &head) {
            continue;
        }
        auto inode = container_of(p, Inode, node);
        if(inode->inode_no == inode_no) {
            increment_rc(&inode->rc);
            // _detach_from_list(p);
            // _insert_into_list(&head, p);
            release_spinlock(&lock);
            return inode;
        }
    }
    Inode* inode = kalloc(sizeof(Inode));
    init_inode(inode);
    inode->inode_no = inode_no;
    increment_rc(&inode->rc);
    inode_lock(inode);
    inode_sync(NULL, inode, false);
    inode_unlock(inode);
    _insert_into_list(&head, &inode->node);
    release_spinlock(&lock);
    return inode;
}
// see `inode.h`.
static void inode_clear(OpContext* ctx, Inode* inode) {
    // TODO
    if(inode->entry.indirect != 0) {
        Block* block = cache->acquire(inode->entry.indirect);
        u32* addrs = get_addrs(block);
        for(usize i = 0; i < INODE_NUM_INDIRECT; i++) {
            if(addrs[i] != 0) {
                cache->free(ctx, addrs[i]);
            }
        }
        cache->release(block);
        cache->free(ctx, inode->entry.indirect);
        inode->entry.indirect = 0;
    }
    for(usize i = 0; i < INODE_NUM_DIRECT; i++) {
        if(inode->entry.addrs[i] != 0) {
            cache->free(ctx, inode->entry.addrs[i]);
            inode->entry.addrs[i] = 0;
        }
    }
    inode->entry.num_bytes = 0;
    inode_sync(ctx, inode, true);
    // usize num_blocks = (inode->entry.num_bytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
    // for(usize i = 0; i < INODE_NUM_DIRECT && num_blocks; i++, num_blocks--){
    //     cache->free(ctx, inode->entry.addrs[i]);
    //     inode->entry.addrs[i] = 0;
    // }
    // if(inode->entry.indirect){
    //     auto indirect_block = cache->acquire(inode->entry.indirect);
    //     u32* addrs = get_addrs(indirect_block);
    //     for(usize i = 0; i < INODE_NUM_INDIRECT && num_blocks; i++, num_blocks--){
    //         cache->free(ctx, addrs[i]);
    //     }
    //     cache->release(indirect_block);
    //     cache->free(ctx, inode->entry.indirect); //free indirect block 
    // }
    // // reset some metadata
    // inode->entry.num_bytes = 0;
    // inode->entry.indirect = 0;
    // inode_sync(ctx, inode, true);
}

// see `inode.h`.
static Inode* inode_share(Inode* inode) {
    // TODO
    increment_rc(&inode->rc);
    return inode;
}

// see `inode.h`.
static void inode_put(OpContext* ctx, Inode* inode) {
    // TODO
    unalertable_wait_sem(&inode->lock);
    decrement_rc(&inode->rc);
    if(inode->rc.count == 0 && inode->entry.num_links == 0) {
        inode->entry.type = INODE_INVALID;
        inode_clear(ctx, inode);
        inode_sync(ctx, inode, true);
        acquire_spinlock(&lock);
        _detach_from_list(&inode->node);
        release_spinlock(&lock);
        post_sem(&inode->lock);
        kfree(inode);
    }
    else {
        post_sem(&inode->lock);
    }
    // inode_lock(inode);
    // if(inode->rc.count == 1 && inode->entry.num_links == 0){
    //     if(inode->entry.num_bytes)inode_clear(ctx, inode);
    //     inode->entry.type = INODE_INVALID;
    //     inode_sync(ctx, inode, true);
    //     acquire_spinlock(&lock);
    //     _detach_from_list(&inode->node);
    //     release_spinlock(&lock);
    //     inode_unlock(inode);
    //     kfree(inode);
    //     return;
    // }
    // inode_unlock(inode);
    // decrement_rc(&inode->rc);
}

/**
    @brief get which block is the offset of the inode in.

    e.g. `inode_map(ctx, my_inode, 1234, &modified)` will return the block_no
    of the block that contains the 1234th byte of the file
    represented by `my_inode`.

    If a block has not been allocated for that byte, `inode_map` will
    allocate a new block and update `my_inode`, at which time, `modified`
    will be set to true.

    HOWEVER, if `ctx == NULL`, `inode_map` will NOT try to allocate any new block,
    and when it finds that the block has not been allocated, it will return 0.
    
    @param[out] modified true if some new block is allocated and `inode`
    has been changed.

    @return usize the block number of that block, or 0 if `ctx == NULL` and
    the required block has not been allocated.

    @note the caller must hold the lock of `inode`.
 */
static usize inode_map(OpContext* ctx,
                       Inode* inode,
                       usize offset,
                       bool* modified) {
    // TODO
    usize num_block = offset;
    if(num_block < INODE_NUM_DIRECT) {
        if(inode->entry.addrs[num_block] == 0) {
            if(ctx == NULL) {
                return 0;
            }
            inode->entry.addrs[num_block] = cache->alloc(ctx);
            *modified = true;
            inode_sync(ctx, inode, true);
        }
        return inode->entry.addrs[num_block];
    }
    num_block -= INODE_NUM_DIRECT;
    if(inode->entry.indirect == 0) {
        if(ctx == NULL) {
            return 0;
        }
        inode->entry.indirect = cache->alloc(ctx);
        inode_sync(ctx, inode, true);
    }
    Block* block = cache->acquire(inode->entry.indirect);
    u32* addrs = get_addrs(block);
    if(addrs[num_block] == 0) {
        if(ctx == NULL) {
            cache->release(block);
            return 0;
        }
        addrs[num_block] = cache->alloc(ctx);
        *modified = true;
        cache->sync(ctx, block);
    }
    cache->release(block);
    return addrs[num_block];
}

// see `inode.h`.
static usize inode_read(Inode* inode, u8* dest, usize offset, usize count) {
    InodeEntry* entry = &inode->entry;
    // printk("inode->entry.type: %d\n", inode->entry.type);
    if(inode->entry.type == INODE_DEVICE) {
        // ASSERT(inode->entry.major == 1);
        return console_read(inode, (char*)dest, count);
    }
    if (count + offset > entry->num_bytes)
        count = entry->num_bytes - offset;
    usize end = offset + count;
    ASSERT(offset <= entry->num_bytes);
    ASSERT(end <= entry->num_bytes);
    ASSERT(offset <= end);

    // TODO
    // if(offset == entry->num_bytes) ASSERT(count == 0);
    for(usize i = offset; i < end; i = (i / BLOCK_SIZE + 1) * BLOCK_SIZE) {
        // printk("i: %lld\n", i);
        bool modified = false;
        usize block_no = inode_map(NULL, inode, i / BLOCK_SIZE, &modified);
        Block* block = cache->acquire(block_no);
        usize len = MIN(end - i, BLOCK_SIZE - i % BLOCK_SIZE);
        // printk("block_no: %lld\n", block_no);
        // printk("len: %lld\n", len);
        // printk("block->data + i mod BLOCK_SIZE: %llx\n", (usize)(block->data + i % BLOCK_SIZE));
        memcpy(dest, block->data + i % BLOCK_SIZE, len);
        dest += len;
        cache->release(block);
    }
    // printk("inode read done\n");
    return count;
}

// see `inode.h`.
static usize inode_write(OpContext* ctx,
                         Inode* inode,
                         u8* src,
                         usize offset,
                         usize count) {
    // printk("in inode_write\n");
    InodeEntry* entry = &inode->entry;
    usize end = offset + count;
    if(inode->entry.type == INODE_DEVICE) {
        // ASSERT(inode->entry.major == 1);
        return console_write(inode, (char*)src, count);
    }
    ASSERT(offset <= entry->num_bytes);
    ASSERT(end <= INODE_MAX_BYTES);
    ASSERT(offset <= end);

    // TODO
    if(end > entry->num_bytes) {
        entry->num_bytes = end;
        inode_sync(ctx, inode, true);
    }
    for(usize i = offset; i < end; i = (i / BLOCK_SIZE + 1) * BLOCK_SIZE) {
        bool modified = false;
        usize block_no = inode_map(ctx, inode, i / BLOCK_SIZE, &modified);
        Block* block = cache->acquire(block_no);
        usize len = MIN(end - i, BLOCK_SIZE - i % BLOCK_SIZE);
        memcpy(block->data + i % BLOCK_SIZE, src, len);
        src += len;
        cache->sync(ctx, block);
        cache->release(block);
    }
    return count;
}

// see `inode.h`.
static usize inode_lookup(Inode* inode, const char* name, usize* index) {
    InodeEntry* entry = &inode->entry;
    ASSERT(entry->type == INODE_DIRECTORY);

    // TODO
    DirEntry curr;
    for(usize i = 0; i < entry->num_bytes; i += sizeof(DirEntry)) {
        inode_read(inode, (u8*)&curr, i, sizeof(DirEntry));
        if(curr.inode_no && memcmp(name, curr.name, MAX(strlen(name), strlen(curr.name))) == 0) {
            if(index != NULL) {
                *index = i;
            }
            return curr.inode_no;
        }
    }
    return 0;
}

// see `inode.h`.
static usize inode_insert(OpContext* ctx,
                          Inode* inode,
                          const char* name,
                          usize inode_no) {
    InodeEntry* entry = &inode->entry;
    ASSERT(entry->type == INODE_DIRECTORY);

    // TODO
    // printk("in inode_insert\n");
    usize offset = entry->num_bytes;
    DirEntry curr;
    for(usize i = 0; i < entry->num_bytes; i += sizeof(DirEntry)) {
        inode_read(inode, (u8*)&curr, i, sizeof(DirEntry));
        if(curr.inode_no == 0 && offset == entry->num_bytes) {
            offset = i;
        }
        if(curr.inode_no && memcmp(name, curr.name, MAX(strlen(name), strlen(curr.name))) == 0) {
            return -1;
        }
    }
    strncpy(curr.name, name, FILE_NAME_MAX_LENGTH);
    curr.inode_no = inode_no;
    // printk("inode->entry.major: %d\n", inode->entry.major);
    inode_write(ctx, inode, (u8*)&curr, offset, sizeof(DirEntry));
    return offset;
}

// see `inode.h`.
static void inode_remove(OpContext* ctx, Inode* inode, usize index) {
    // TODO
    // printk("in inode_remove\n");
    DirEntry curr;
    inode_read(inode, (u8*)&curr, index, sizeof(DirEntry));
    curr.inode_no = 0;
    inode_write(ctx, inode, (u8*)&curr, index, sizeof(DirEntry));
}

InodeTree inodes = {
    .alloc = inode_alloc,
    .lock = inode_lock,
    .unlock = inode_unlock,
    .sync = inode_sync,
    .get = inode_get,
    .clear = inode_clear,
    .share = inode_share,
    .put = inode_put,
    .read = inode_read,
    .write = inode_write,
    .lookup = inode_lookup,
    .insert = inode_insert,
    .remove = inode_remove,
};

/**
    @brief read the next path element from `path` into `name`.
    
    @param[out] name next path element.

    @return const char* a pointer offseted in `path`, without leading `/`. If no
    name to remove, return NULL.

    @example 
    skipelem("a/bb/c", name) = "bb/c", setting name = "a",
    skipelem("///a//bb", name) = "bb", setting name = "a",
    skipelem("a", name) = "", setting name = "a",
    skipelem("", name) = skipelem("////", name) = NULL, not setting name.
 */
static const char* skipelem(const char* path, char* name) {
    const char* s;
    int len;

    while (*path == '/')
        path++;
    if (*path == 0)
        return 0;
    s = path;
    while (*path != '/' && *path != 0)
        path++;
    len = path - s;
    if (len >= FILE_NAME_MAX_LENGTH)
        memmove(name, s, FILE_NAME_MAX_LENGTH);
    else {
        memmove(name, s, len);
        name[len] = 0;
    }
    while (*path == '/')
        path++;
    return path;
}

/**
    @brief look up and return the inode for `path`.

    If `nameiparent`, return the inode for the parent and copy the final
    path element into `name`.
    
    @param path a relative or absolute path. If `path` is relative, it is
    relative to the current working directory of the process.

    @param[out] name the final path element if `nameiparent` is true.

    @return Inode* the inode for `path` (or its parent if `nameiparent` is true), 
    or NULL if such inode does not exist.

    @example
    namex("/a/b", false, name) = inode of b,
    namex("/a/b", true, name) = inode of a, setting name = "b",
    namex("/", true, name) = NULL (because "/" has no parent!)
 */
static Inode* namex(const char* path,
                    bool nameiparent,
                    char* name,
                    OpContext* ctx) {
    /* (Final) TODO BEGIN */
    // printk("in namex: path: %s\n", path);
    Inode *ans;
    // Inode *next;
    if(*path == '/') {
        ans = inode_get(ROOT_INODE_NO);
    }else {
        ans = inode_share(thisproc()->cwd);
    }
    while((path = skipelem(path, name)) != NULL) {
        inode_lock(ans);
        // printk("ans->entry.type: %d\n", ans->entry.type);
        if(ans->entry.type != INODE_DIRECTORY) {
            inode_unlock(ans);
            inode_put(ctx, ans);
            return NULL;
        }
        if(nameiparent && *path == '\0') {
            inode_unlock(ans);
            return ans;
        }
        usize inode_no = inode_lookup(ans, name, 0);
        // printk("in namex: inode_no: %lld\n", inode_no);
        if(inode_no == 0) {
            inode_unlock(ans);
            inode_put(ctx, ans);
            return NULL;
        }
        inode_unlock(ans);
        inode_put(ctx, ans);
        ans = inode_get(inode_no);
    }
    if(nameiparent) {
        inode_put(ctx, ans);
        return NULL;
    }
    return ans;
    /* (Final) TODO END */
}

Inode* namei(const char* path, OpContext* ctx) {
    // printk("in namei\n");
    char name[FILE_NAME_MAX_LENGTH];
    return namex(path, false, name, ctx);
}

Inode* nameiparent(const char* path, char* name, OpContext* ctx) {
    // printk("in nameiparent\n");
    return namex(path, true, name, ctx);
}

/**
    @brief get the stat information of `ip` into `st`.
    
    @note the caller must hold the lock of `ip`.
 */
void stati(Inode* ip, struct stat* st) {
    st->st_dev = 1;
    st->st_ino = ip->inode_no;
    st->st_nlink = ip->entry.num_links;
    st->st_size = ip->entry.num_bytes;
    switch (ip->entry.type) {
        case INODE_REGULAR:
            st->st_mode = S_IFREG;
            break;
        case INODE_DIRECTORY:
            st->st_mode = S_IFDIR;
            break;
        case INODE_DEVICE:
            st->st_mode = 0;
            break;
        default:
            PANIC();
    }
}