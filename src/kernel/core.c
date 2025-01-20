#include <aarch64/intrinsic.h>
#include <kernel/cpu.h>
#include <kernel/printk.h>
#include <kernel/sched.h>
#include <test/test.h>
#include <common/buf.h>
#include <driver/virtio.h>
#include <kernel/proc.h>

volatile bool panic_flag;
extern char icode[];
extern char eicode[];
void trap_return();

NO_RETURN void idle_entry()
{
    set_cpu_on();
    while (1) {
        yield();
        if (panic_flag)
            break;
        arch_with_trap
        {
            arch_wfi();
        }
    }
    set_cpu_off();
    arch_stop_cpu();
}

NO_RETURN void kernel_entry()
{
    init_filesystem();

    printk("Hello world! (Core %lld)\n", cpuid());
    // proc_test();
    // vm_test();
    // user_proc_test();
    // io_test();

    /* LAB 4 TODO 3 BEGIN */
    // Buf MBR_buf;
    // MBR_buf.flags = 0;
    // MBR_buf.block_no = 0;
    // virtio_blk_rw(&MBR_buf);

    // u32* LBA = (u32*)(MBR_buf.data + 0x1CE + 0x8);
    // u32* sectors = (u32*)(MBR_buf.data + 0x1CE + 0xC);
    // printk("LBA: %d, sectors: %d\n", *LBA, *sectors);
    /* LAB 4 TODO 3 END */

    /**
     * (Final) TODO BEGIN 
     * 
     * Map init.S to user space and trap_return to run icode.
     */
    Proc* p = create_proc();
    // printk("Create Proc\n");
    for(u64 q = (u64)icode; q < (u64)eicode; q += PAGE_SIZE){
        auto pte = get_pte(&p->pgdir, 0x400000 + q - (u64)icode, 1);
        *pte = K2P(q) | PTE_USER_DATA;
    }
    ASSERT(p->pgdir.pt);
    p->ucontext->x[0] = 0;
    p->ucontext->elr = 0x400000;
    p->ucontext->spsr = 0;
    OpContext ctx;
    bcache.begin_op(&ctx);
    // printk("Begin op\n");
    p->cwd = namei("/", &ctx);
    // printk("Namei\n");
    bcache.end_op(&ctx);
    // printk("End op\n");
    set_parent_to_this(p);
    start_proc(p, trap_return, 0);
    // printk("Start\n");
    while(1){
        yield();
        arch_with_trap{
            arch_wfi();
        }
    }
    /* (Final) TODO END */
}

NO_INLINE NO_RETURN void _panic(const char *file, int line)
{
    printk("=====%s:%d PANIC%lld!=====\n", file, line, cpuid());
    panic_flag = true;
    set_cpu_off();
    for (int i = 0; i < NCPU; i++) {
        if (cpus[i].online)
            i--;
    }
    printk("Kernel PANIC invoked at %s:%d. Stopped.\n", file, line);
    arch_stop_cpu();
}