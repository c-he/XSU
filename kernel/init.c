#include <arch.h>
#include <driver/ps2.h>
#include <driver/vga.h>
#include <exc.h>
#include <intr.h>
#include <xsu/fs/fat.h>
#include <xsu/log.h>
#include <xsu/syscall.h>
#include <xsu/time.h>
#include "../usr/ps.h"
#include <init_place_holder.h>

void machine_info()
{
    int row;
    int col;
    kernel_printf("\n%s\n", "XSU File System Test V1.0");
    row = cursor_row;
    col = cursor_col;
    cursor_row = 29;
    kernel_printf("%s", "Created by Deep Dark Fantasy, Zhejiang University.");
    cursor_row = row;
    cursor_col = col;
    kernel_set_cursor();
}

void init_kernel()
{
    kernel_clear_screen(31);
    // Exception
    init_exception();
    // Page table
    init_pgtable();
    // Drivers
    init_vga();
    init_ps2();
    init_time();
    // Memory management
    // log(LOG_START, "Memory Modules.");
    init_mem();
    // log(LOG_END, "Memory Modules.");
    // File system
    log(LOG_START, "File System.");
    init_fs();
    log(LOG_END, "File System.");
    // System call
    log(LOG_START, "System Calls.");
    init_syscall();
    log(LOG_END, "System Calls.");
    // Process control
    // log(LOG_START, "Process Control Module.");
    init_pc();
    // log(LOG_END, "Process Control Module.");
    // Interrupts
    log(LOG_START, "Enable Interrupts.");
    init_interrupts();
    log(LOG_END, "Enable Interrupts.");
    // Init finished
    machine_info();
    *GPIO_SEG = 0x11223344;
    // Enter shell
    ps();
}
