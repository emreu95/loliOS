#include "idt.h"
#include "lib.h"
#include "debug.h"
#include "x86_desc.h"

/* IRQ handler array */
static irq_handler_t irq_handlers[16];

/* Exception info table */
static exc_info_t exc_info_table[20] = {
    {EXC_DE, "Divide error exception"},
    {EXC_DB, "Debug exception"},
    {EXC_NI, "Nonmaskable interrupt"},
    {EXC_BP, "Breakpoint exception"},
    {EXC_OF, "Overflow exception"},
    {EXC_BR, "Bound range exceeded exception"},
    {EXC_UD, "Invalid opcode exception"},
    {EXC_NM, "Device not available exception"},
    {EXC_DF, "Double fault exception"},
    {EXC_CO, "Coprocessor segment overrun"},
    {EXC_TS, "Invalid TSS exception"},
    {EXC_NP, "Segment not present"},
    {EXC_SS, "Stack fault exception"},
    {EXC_GP, "General protection exception"},
    {EXC_PF, "Page-fault exception"},
    {EXC_RE, "Entry reserved"},
    {EXC_MF, "Floating-point error"},
    {EXC_AC, "Alignment check exception"},
    {EXC_MC, "Machine-check exception"},
    {EXC_XF, "SIMD floating-point exception"},
};

/* Convenience wrapper around SET_IDT_ENTRY */
#define WRITE_IDT_ENTRY(i, name)               \
do {                                           \
    extern void name(void);                    \
    SET_IDT_ENTRY(idt[i], name);               \
} while (0)

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

/* Exception handler */
static void
handle_exception(int_regs_t *regs)
{
    clear();
    printf("****************************************\n");
    printf("Exception: %s\n", exc_info_table[regs->int_num].desc);
    printf("****************************************\n");
    dump_registers(regs);
    halt();
}

/* IRQ handler */
static void
handle_irq(int_regs_t *regs)
{
    uint32_t irq_num = regs->int_num - INT_IRQ0;
    irq_handler_t handler = irq_handlers[irq_num];
    debugf("IRQ interrupt: %d\n", irq_num);

    /* Run callback if it's registered */
    if (handler.callback != NULL) {
        handler.callback();
    }

    /* Clear interrupt flag on PIC */
    send_eoi(irq_num);
}

/* Syscall handler */
static void
handle_syscall(int_regs_t *regs)
{
    debugf("Syscall: %d\n", regs->eax);
    switch (regs->eax) {
        /* TODO */
    }
}

/*
 * Called when an interrupt occurs (from idtthunk.S).
 * The registers in regs should not be modified unless
 * the interrupt is a syscall.
 */
void
handle_interrupt(int_regs_t *regs)
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
}

/*
 * Registers an IRQ handler.
 *
 * Currently only one handler can be registered per IRQ line.
 */
void
register_irq_handler(uint32_t irq_num, void (*callback)(void))
{
    ASSERT(irq_num >= 0 && irq_num < 16);
    irq_handlers[irq_num].callback = callback;
    enable_irq(irq_num);
}

/*
 * Unregisters a IRQ handler.
 */
void
unregister_irq_handler(uint32_t irq_num)
{
    ASSERT(irq_num >= 0 && irq_num < 16);
    disable_irq(irq_num);
    irq_handlers[irq_num].callback = NULL;
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
    WRITE_IDT_ENTRY(EXC_DE, handle_exc_de);
    WRITE_IDT_ENTRY(EXC_DB, handle_exc_db);
    WRITE_IDT_ENTRY(EXC_NI, handle_exc_ni);
    WRITE_IDT_ENTRY(EXC_BP, handle_exc_bp);
    WRITE_IDT_ENTRY(EXC_OF, handle_exc_of);
    WRITE_IDT_ENTRY(EXC_BR, handle_exc_br);
    WRITE_IDT_ENTRY(EXC_UD, handle_exc_ud);
    WRITE_IDT_ENTRY(EXC_NM, handle_exc_nm);
    WRITE_IDT_ENTRY(EXC_DF, handle_exc_df);
    WRITE_IDT_ENTRY(EXC_CO, handle_exc_co);
    WRITE_IDT_ENTRY(EXC_TS, handle_exc_ts);
    WRITE_IDT_ENTRY(EXC_NP, handle_exc_np);
    WRITE_IDT_ENTRY(EXC_SS, handle_exc_ss);
    WRITE_IDT_ENTRY(EXC_GP, handle_exc_gp);
    WRITE_IDT_ENTRY(EXC_PF, handle_exc_pf);
    WRITE_IDT_ENTRY(EXC_RE, handle_exc_re);
    WRITE_IDT_ENTRY(EXC_MF, handle_exc_mf);
    WRITE_IDT_ENTRY(EXC_AC, handle_exc_ac);
    WRITE_IDT_ENTRY(EXC_MC, handle_exc_mc);
    WRITE_IDT_ENTRY(EXC_XF, handle_exc_xf);

    /* Initialize interrupt gates */
    desc.dpl = 0;
    desc.reserved3 = 0;
    for (; i < NUM_VEC; ++i) {
        idt[i] = desc;
        WRITE_IDT_ENTRY(i, handle_int_unknown);
    }

    /* Initialize IRQ interrupt gates */
    WRITE_IDT_ENTRY(INT_IRQ0, handle_int_irq0);
    WRITE_IDT_ENTRY(INT_IRQ1, handle_int_irq1);
    WRITE_IDT_ENTRY(INT_IRQ2, handle_int_irq2);
    WRITE_IDT_ENTRY(INT_IRQ3, handle_int_irq3);
    WRITE_IDT_ENTRY(INT_IRQ4, handle_int_irq4);
    WRITE_IDT_ENTRY(INT_IRQ5, handle_int_irq5);
    WRITE_IDT_ENTRY(INT_IRQ6, handle_int_irq6);
    WRITE_IDT_ENTRY(INT_IRQ7, handle_int_irq7);
    WRITE_IDT_ENTRY(INT_IRQ8, handle_int_irq8);
    WRITE_IDT_ENTRY(INT_IRQ9, handle_int_irq9);
    WRITE_IDT_ENTRY(INT_IRQ10, handle_int_irq10);
    WRITE_IDT_ENTRY(INT_IRQ11, handle_int_irq11);
    WRITE_IDT_ENTRY(INT_IRQ12, handle_int_irq12);
    WRITE_IDT_ENTRY(INT_IRQ13, handle_int_irq13);
    WRITE_IDT_ENTRY(INT_IRQ14, handle_int_irq14);
    WRITE_IDT_ENTRY(INT_IRQ15, handle_int_irq15);

    /* Initialize syscall interrupt gate */
    idt[INT_SYSCALL].dpl = 3;
    WRITE_IDT_ENTRY(INT_SYSCALL, handle_int_syscall);
}
