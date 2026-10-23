#pragma once
#include <stdint.h>

/*
 * everything the interrupt stub (isr_common) saves on the stack, in memory
 * order. the last three get pushed by the cpu itself before the stub even
 * starts.
 */
struct regs {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
};

typedef void (*irq_handler_t)(struct regs *r);

void gdt_init(void);
void idt_init(void);

void pic_init(void);
void pic_unmask(uint8_t irq);
void pic_send_eoi(uint8_t irq);
void irq_register(uint8_t irq, irq_handler_t handler);

void timer_init(uint32_t hz);
uint32_t timer_ticks(void);
uint32_t timer_hz(void);

/*
 * keyboard_getchar waits for a key; keyboard_poll gives up straight away and
 * returns 0 if there isn't one.
 */
void keyboard_init(void);
char keyboard_getchar(void);
int keyboard_poll(char *c);

void panic(const char *msg) __attribute__((noreturn));
