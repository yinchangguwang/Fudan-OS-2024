#include <aarch64/intrinsic.h>
#include <common/string.h>
#include <driver/uart.h>
#include <kernel/core.h>
#include <kernel/mem.h>
#include <kernel/printk.h>

static volatile bool boot_secondary_cpus = false;

void main() {
    if (cpuid() == 0) {
        /* @todo: Clear BSS section.*/
        extern char edata[], end[];
        memset(edata, 0, (usize)(end - edata));

        smp_init();
        uart_init();
        printk_init();

        /* initialize kernel memory allocator */
        kinit();
        
        /* @todo: Print "Hello, world! (Core 0)" */
        printk("Hello, world! (Core 0)\n");

        arch_fence();

        // Set a flag indicating that the secondary CPUs can start executing.
        boot_secondary_cpus = true;
    } else {
        while (!boot_secondary_cpus);
        arch_fence();
    
        /* @todo: Print "Hello, world! (Core <core id>)" */
        printk("Hello, world! (Core %llu)\n", cpuid());
    }

    set_return_addr(idle_entry);
}
