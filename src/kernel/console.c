#include <kernel/console.h>
#include <aarch64/intrinsic.h>
#include <kernel/sched.h>
#include <driver/uart.h>
#include <driver/interrupt.h>
#include <kernel/printk.h>

#define BACKSPACE 0x100
#define MAX_HISTORY 10
#define MAX_CMD_LEN 256

struct console cons;
// char history[MAX_HISTORY][MAX_CMD_LEN];
// int history_count = 0;
// int history_index = -1;
// usize cursor_pos = 0;

void console_init()
{
    /* (Final) TODO BEGIN */
    // printk("in console_init\n");
    init_spinlock(&cons.lock);
    init_sem(&cons.sem, 0);
    set_interrupt_handler(IRQ_AUX, console_intr2);
    /* (Final) TODO END */
}

void putc(int c){
    if(c == BACKSPACE) {
        uart_put_char('\b');
        uart_put_char(' ');
        uart_put_char('\b');
    }else{
        uart_put_char(c);
    }
}

// void move_cursor_left() {
//     if (cursor_pos > 0) {
//         cursor_pos--;
//         uart_put_char('\b');
//     }
// }

// void move_cursor_right() {
//     if (cursor_pos < cons.edit_idx) {
//         uart_put_char(cons.buf[cursor_pos % IBUF_SIZE]);
//         cursor_pos++;
//     }
// }

// void load_history(int index) {
//     if (index >= 0 && index < history_count) {
//         while (cons.edit_idx > 0) {
//             cons.edit_idx--;
//             putc(BACKSPACE);
//         }
//         strncpy(cons.buf, history[index], MAX_CMD_LEN);
//         cons.edit_idx = strlen(history[index]);
//         cursor_pos = cons.edit_idx;
//         for (usize i = 0; i < cons.edit_idx; i++) {
//             putc(cons.buf[i]);
//         }
//     }
// }

/**
 * console_write - write to uart from the console buffer.
 * @ip: the pointer to the inode
 * @buf: the buffer
 * @n: number of bytes to write
 */
isize console_write(Inode *ip, char *buf, isize n)
{
    /* (Final) TODO BEGIN */
    // printk("in console_write\n");
    inodes.unlock(ip);
    acquire_spinlock(&cons.lock);
    for(isize i = 0; i < n; i++){
        putc(buf[i]);
    }
    release_spinlock(&cons.lock);
    inodes.lock(ip);
    return n;
    /* (Final) TODO END */
}

/**
 * console_read - read to the destination from the buffer
 * @ip: the pointer to the inode
 * @dst: the destination
 * @n: number of bytes to read
 */
isize console_read(Inode *ip, char *dst, isize n)
{
    /* (Final) TODO BEGIN */
    // printk("in console_read\n");
    inodes.unlock(ip);
    acquire_spinlock(&cons.lock);
    isize m = n;
    while(n > 0){
        while(cons.read_idx == cons.write_idx){
            if(thisproc()->killed){
                release_spinlock(&cons.lock);
                inodes.lock(ip);
                return -1;
            }
            release_spinlock(&cons.lock);
            unalertable_wait_sem(&cons.sem);
            // acquire_spinlock(&cons.lock);
        }
        int c = cons.buf[cons.read_idx % IBUF_SIZE];
        cons.read_idx++;
        if(c == C('D')){
            if(n < m){
                cons.read_idx--;
            }
            break;
        }
        *dst = c;
        dst++;
        n--;
        if(c == '\n'){
            break;
        }
    }
    release_spinlock(&cons.lock);
    inodes.lock(ip);
    return m - n;
    /* (Final) TODO END */
}

void console_intr2(){
    char c = uart_get_char();
    console_intr(c);
}

void console_intr(char c)
{
    /* (Final) TODO BEGIN */
    // printk("in console_intr\n");
    acquire_spinlock(&cons.lock);
    if(c){
        if(c == C('C')){
            ASSERT(kill(thisproc()->pid) == 0);
        }else if(c == C('U')){
            while(cons.edit_idx != cons.write_idx && cons.buf[(cons.edit_idx - 1) % IBUF_SIZE] != '\n'){
                cons.edit_idx--;
                putc(BACKSPACE);
            }
        }else if(c == '\x7f'){
            if(cons.edit_idx != cons.write_idx){
                cons.edit_idx--;
                putc(BACKSPACE);
            }
        }else{
            if(c != 0 && cons.edit_idx - cons.read_idx < IBUF_SIZE){
                c = (c == '\r') ? '\n' : c;
                cons.buf[cons.edit_idx % IBUF_SIZE] = c;
                cons.edit_idx++;
                putc(c);
                if(c == '\n' || c == C('D') || cons.edit_idx == cons.read_idx + IBUF_SIZE){
                    cons.write_idx = cons.edit_idx;
                    post_sem(&cons.sem);
                }
            }
        }
    }
    release_spinlock(&cons.lock);
    /* (Final) TODO END */
}