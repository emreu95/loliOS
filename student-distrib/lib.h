#ifndef _LIB_H
#define _LIB_H

#include "types.h"

int32_t printf(int8_t *format, ...);
void putc(uint8_t c);
int32_t puts(int8_t *s);
int32_t *atoi_s(const int8_t *value, int32_t *out_result);
int8_t *itoa(uint32_t value, int8_t* buf, int32_t radix);
int8_t *strrev(int8_t* s);
uint32_t strlen(const int8_t* s);
void clear(void);

void* memset(void* s, int32_t c, uint32_t n);
void* memset_word(void* s, int32_t c, uint32_t n);
void* memset_dword(void* s, int32_t c, uint32_t n);
void* memcpy(void* dest, const void* src, uint32_t n);
void* memmove(void* dest, const void* src, uint32_t n);
int32_t strcmp(const int8_t* s1, const int8_t* s2);
int32_t strncmp(const int8_t* s1, const int8_t* s2, uint32_t n);
int8_t* strcpy(int8_t* dest, const int8_t*src);
int8_t* strncpy(int8_t* dest, const int8_t*src, uint32_t n);
bool is_user_readable_string(const uint8_t *str);
bool is_user_readable(const void *user_buf, int32_t n);
bool is_user_writable(const void *user_buf, int32_t n);
int32_t read_char_from_user(const uint8_t *ptr);
bool strncpy_from_user(uint8_t *dest, const uint8_t *src, uint32_t n);
bool copy_from_user(void *dest, const void *src, int32_t n);
bool copy_to_user(void *dest, const void *src, int32_t n);

/* Puts the processor into an infinite loop */
void loop(void);

/* Port read functions */
/* Inb reads a byte and returns its value as a zero-extended 32-bit
 * unsigned int */
static inline uint32_t inb(port)
{
    uint32_t val;
    asm volatile("xorl %0, %0\n \
            inb   (%w1), %b0"
            : "=a"(val)
            : "d"(port)
            : "memory" );
    return val;
}

/* Reads two bytes from two consecutive ports, starting at "port",
 * concatenates them little-endian style, and returns them zero-extended
 * */
static inline uint32_t inw(port)
{
    uint32_t val;
    asm volatile("xorl %0, %0\n   \
            inw   (%w1), %w0"
            : "=a"(val)
            : "d"(port)
            : "memory" );
    return val;
}

/* Reads four bytes from four consecutive ports, starting at "port",
 * concatenates them little-endian style, and returns them */
static inline uint32_t inl(port)
{
    uint32_t val;
    asm volatile("inl   (%w1), %0"
            : "=a"(val)
            : "d"(port)
            : "memory" );
    return val;
}

/* Writes a byte to a port */
#define outb(data, port)                \
do {                                    \
    asm volatile("outb  %b1, (%w0)"     \
            :                           \
            : "d" (port), "a" (data)    \
            : "memory", "cc" );         \
} while(0)

/* Writes two bytes to two consecutive ports */
#define outw(data, port)                \
do {                                    \
    asm volatile("outw  %w1, (%w0)"     \
            :                           \
            : "d" (port), "a" (data)    \
            : "memory", "cc" );         \
} while(0)

/* Writes four bytes to four consecutive ports */
#define outl(data, port)                \
do {                                    \
    asm volatile("outl  %l1, (%w0)"     \
            :                           \
            : "d" (port), "a" (data)    \
            : "memory", "cc" );         \
} while(0)

/* Clear interrupt flag - disables interrupts on this processor */
#define cli()                           \
do {                                    \
    asm volatile("cli"                  \
            :                       \
            :                       \
            : "memory", "cc"        \
            );                      \
} while(0)

/* Save flags and then clear interrupt flag
 * Saves the EFLAGS register into the variable "flags", and then
 * disables interrupts on this processor */
#define cli_and_save(flags)             \
do {                                    \
    asm volatile("pushfl        \n      \
                popl %0         \n      \
                cli"                    \
            : "=r"(flags)           \
            :                       \
            : "memory", "cc"        \
            );                      \
} while(0)

/* Save flags and then set interrupt flag
 * Saves the EFLAGS register into the variable "flags", and then
 * enables interrupts on this processor */
#define sti_and_save(flags)             \
do {                                    \
    asm volatile("pushfl        \n      \
                popl %0         \n      \
                sti"                    \
            : "=r"(flags)              \
            :                          \
            : "memory", "cc"           \
            );                         \
} while(0)

/* Set interrupt flag - enable interrupts on this processor */
#define sti()                           \
do {                                    \
    asm volatile("sti"                  \
            :                       \
            :                       \
            : "memory", "cc"        \
            );                      \
} while(0)

/* Halt - waits for an interrupt to occur */
#define hlt()                               \
do {                                        \
    asm volatile("hlt");                    \
} while(0)

/* Restore flags
 * Puts the value in "flags" into the EFLAGS register.  Most often used
 * after a cli_and_save_flags(flags) */
#define restore_flags(flags)            \
do {                                    \
    asm volatile("pushl %0      \n      \
            popfl"                  \
            :                       \
            : "r"(flags)            \
            : "memory", "cc"        \
            );                      \
} while(0)

/*
 * Read the contents of the specified register
 * and stores it into dest.
 */
#define read_register(name, dest)                       \
do {                                                    \
    asm volatile("movl %%" name ", %0" : "=g"(dest));   \
} while (0)

#endif /* _LIB_H */
