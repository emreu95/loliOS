#include "idt.h"
#include "lib.h"
#include "debug.h"
#include "x86_desc.h"
#include "irq.h"
#include "syscall.h"
#include "process.h"
#include "signal.h"

/* Convenience wrapper around SET_IDT_ENTRY */
#define WRITE_IDT_ENTRY(i, name)            \
do {                                        \
    extern void name(void);                 \
    SET_IDT_ENTRY(idt[i], name);            \
} while (0)

/* Exception number to name table */
static const char *exception_names[] = {
    "Divide error exception",
    "Debug exception",
    "Nonmaskable interrupt",
    "Breakpoint exception",
    "Overflow exception",
    "Bound range exceeded exception",
    "Invalid opcode exception",
    "Device not available exception",
    "Double fault exception",
    "Coprocessor segment overrun",
    "Invalid TSS exception",
    "Segment not present",
    "Stack fault exception",
    "General protection exception",
    "Page-fault exception",
    "Entry reserved",
    "Floating-point error",
    "Alignment check exception",
    "Machine-check exception",
    "SIMD floating-point exception",
};

/* Prints all interrupt registers */
static void
dump_registers(int_regs_t *regs)
{
    printf("int_num:    0x%#x\n", regs->int_num);
    printf("error_code: 0x%#x\n", regs->error_code);
    printf("eax:        0x%#x\n", regs->eax);
    printf("ebx:        0x%#x\n", regs->ebx);
    printf("ecx:        0x%#x\n", regs->ecx);
    printf("edx:        0x%#x\n", regs->edx);
    printf("esi:        0x%#x\n", regs->esi);
    printf("edi:        0x%#x\n", regs->edi);
    printf("ebp:        0x%#x\n", regs->ebp);
    printf("esp:        0x%#x\n", regs->esp);
    printf("eip:        0x%#x\n", regs->eip);
    printf("eflags:     0x%#x\n", regs->eflags);
    printf("cs:         0x%#x\n", regs->cs);
    printf("ds:         0x%#x\n", regs->ds);
    printf("es:         0x%#x\n", regs->es);
    printf("fs:         0x%#x\n", regs->fs);
    printf("gs:         0x%#x\n", regs->gs);
    printf("ss:         0x%#x\n", regs->ss);
    printf("cr0:        0x%#x\n", regs->cr0);
    printf("cr2:        0x%#x\n", regs->cr2);
    printf("cr3:        0x%#x\n", regs->cr3);
    printf("cr4:        0x%#x\n", regs->cr4);
}

/*
 * Handles an exception that occurred in userspace.
 * If a signal handler is available, will cause that to
 * be executed. Otherwise, kills the process.
 */
static void
handle_user_exception(uint32_t int_num)
{
    debugf("Userspace exception: %s\n", exception_names[int_num]);
    pcb_t *pcb = get_executing_pcb();
    if (int_num == EXC_DE) {
        signal_raise(pcb->pid, SIG_DIV_ZERO);
    } else {
        signal_raise(pcb->pid, SIG_SEGFAULT);
    }
}

/* Exception handler */
static void
handle_exception(int_regs_t *regs)
{
    /* If we were in userspace, run signal handler or kill the process */
    if (regs->cs == USER_CS) {
        handle_user_exception(regs->int_num);
        return;
    }

    clear();
    printf("****************************************\n");
    printf("Exception: %s\n", exception_names[regs->int_num]);
    printf("****************************************\n");
    dump_registers(regs);
    loop();
}

/* IRQ handler */
static void
handle_irq(int_regs_t *regs)
{
    uint32_t irq_num = regs->int_num - INT_IRQ0;
    irq_handle_interrupt(irq_num);
}

/* Syscall handler */
static void
handle_syscall(int_regs_t *regs)
{
    debugf("Syscall: %d\n", regs->eax);
    regs->eax = syscall_handle(regs->ebx, regs->ecx, regs->edx, regs, regs->eax);
    debugf("Return value: 0x%#x\n", regs->eax);
}

/*
 * Called when an interrupt occurs (from idtthunk.S).
 * The registers in regs should not be modified unless
 * the interrupt is a syscall.
 */
__cdecl void
idt_handle_interrupt(int_regs_t *regs)
{
    if (regs->int_num < NUM_EXC) {
        handle_exception(regs);
    } else if (regs->int_num >= INT_IRQ0 && regs->int_num <= INT_IRQ15) {
        handle_irq(regs);
    } else if (regs->int_num == INT_SYSCALL) {
        handle_syscall(regs);
    } else {
        debugf("Unknown interrupt: %d\n", regs->int_num);
    }

    /*
     * If process has any pending signals, run their handlers.
     * Note that since we have security checks inside sigreturn,
     * we only do this if we came from userspace, since that's
     * the only place we can safely return to after sigreturn.
     */
    if (regs->cs == USER_CS) {
        signal_handle_all(regs);
    }
}

/*
 * Initializes the interrupt descriptor table.
 */
void
idt_init(void)
{
    idt_desc_t desc;
    int i;

    /* Initialize template interrupt descriptor */
    desc.present      = 1;
    desc.dpl          = 0;
    desc.reserved0    = 0;
    desc.size         = 1;
    desc.reserved1    = 1;
    desc.reserved2    = 1;
    desc.reserved3    = 1;
    desc.reserved4    = 0;
    desc.seg_selector = KERNEL_CS;
    desc.offset_15_00 = 0;
    desc.offset_31_16 = 0;

    /* Load the IDT */
    lidt(idt_desc_ptr);

    /* Initialize exception (trap) gates */
    desc.dpl = 0;

    /* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     * Use 0 (interrupt gate) for now. In order
     * to switch to an actual trap gate in the
     * future, change this from 0 to 1
     * !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
     */
    desc.reserved3 = 0;
    for (i = 0; i < NUM_EXC; ++i) {
        idt[i] = desc;
    }

    /* Write exception handlers */
    WRITE_IDT_ENTRY(EXC_DE, idt_handle_exc_de);
    WRITE_IDT_ENTRY(EXC_DB, idt_handle_exc_db);
    WRITE_IDT_ENTRY(EXC_NI, idt_handle_exc_ni);
    WRITE_IDT_ENTRY(EXC_BP, idt_handle_exc_bp);
    WRITE_IDT_ENTRY(EXC_OF, idt_handle_exc_of);
    WRITE_IDT_ENTRY(EXC_BR, idt_handle_exc_br);
    WRITE_IDT_ENTRY(EXC_UD, idt_handle_exc_ud);
    WRITE_IDT_ENTRY(EXC_NM, idt_handle_exc_nm);
    WRITE_IDT_ENTRY(EXC_DF, idt_handle_exc_df);
    WRITE_IDT_ENTRY(EXC_CO, idt_handle_exc_co);
    WRITE_IDT_ENTRY(EXC_TS, idt_handle_exc_ts);
    WRITE_IDT_ENTRY(EXC_NP, idt_handle_exc_np);
    WRITE_IDT_ENTRY(EXC_SS, idt_handle_exc_ss);
    WRITE_IDT_ENTRY(EXC_GP, idt_handle_exc_gp);
    WRITE_IDT_ENTRY(EXC_PF, idt_handle_exc_pf);
    WRITE_IDT_ENTRY(EXC_RE, idt_handle_exc_re);
    WRITE_IDT_ENTRY(EXC_MF, idt_handle_exc_mf);
    WRITE_IDT_ENTRY(EXC_AC, idt_handle_exc_ac);
    WRITE_IDT_ENTRY(EXC_MC, idt_handle_exc_mc);
    WRITE_IDT_ENTRY(EXC_XF, idt_handle_exc_xf);

    /* Initialize interrupt gates */
    desc.dpl = 0;
    desc.reserved3 = 0;
    for (; i < NUM_VEC; ++i) {
        idt[i] = desc;
        WRITE_IDT_ENTRY(i, idt_handle_int_unknown);
    }

    /* Initialize IRQ interrupt gates */
    WRITE_IDT_ENTRY(INT_IRQ0, idt_handle_int_irq0);
    WRITE_IDT_ENTRY(INT_IRQ1, idt_handle_int_irq1);
    WRITE_IDT_ENTRY(INT_IRQ2, idt_handle_int_irq2);
    WRITE_IDT_ENTRY(INT_IRQ3, idt_handle_int_irq3);
    WRITE_IDT_ENTRY(INT_IRQ4, idt_handle_int_irq4);
    WRITE_IDT_ENTRY(INT_IRQ5, idt_handle_int_irq5);
    WRITE_IDT_ENTRY(INT_IRQ6, idt_handle_int_irq6);
    WRITE_IDT_ENTRY(INT_IRQ7, idt_handle_int_irq7);
    WRITE_IDT_ENTRY(INT_IRQ8, idt_handle_int_irq8);
    WRITE_IDT_ENTRY(INT_IRQ9, idt_handle_int_irq9);
    WRITE_IDT_ENTRY(INT_IRQ10, idt_handle_int_irq10);
    WRITE_IDT_ENTRY(INT_IRQ11, idt_handle_int_irq11);
    WRITE_IDT_ENTRY(INT_IRQ12, idt_handle_int_irq12);
    WRITE_IDT_ENTRY(INT_IRQ13, idt_handle_int_irq13);
    WRITE_IDT_ENTRY(INT_IRQ14, idt_handle_int_irq14);
    WRITE_IDT_ENTRY(INT_IRQ15, idt_handle_int_irq15);

    /* Initialize syscall interrupt gate */
    idt[INT_SYSCALL].dpl = 3;
    WRITE_IDT_ENTRY(INT_SYSCALL, idt_handle_int_syscall);
}
