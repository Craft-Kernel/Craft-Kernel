#include <cpu.h>
#include <io.h>
#include <stdbool.h>

/*
 * the ps/2 keyboard, on irq 1, with a us layout and scancode set 1.
 *
 * every key sends a "make" code when it's pressed, and the same code with bit
 * 7 set when it's let go. we keep track of shift and caps lock ourselves and
 * turn presses into ascii, which goes into a small ring buffer for whoever's
 * reading the keyboard. extended keys (the arrows, right ctrl and so on)
 * start with 0xe0, and they're ignored for now.
 */

#define KBD_DATA 0x60
#define BUF_SIZE 128

static const char keymap[128] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ',
};

static const char keymap_shift[128] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ',
};

#define SC_LSHIFT     0x2A
#define SC_RSHIFT     0x36
#define SC_CAPSLOCK   0x3A
#define SC_EXTENDED   0xE0
#define SC_RELEASE    0x80

static bool shift, caps, extended;

/*
 * only the interrupt handler moves head, and only the reader moves tail, so
 * the two never step on each other and no locking is needed. if the buffer
 * fills up because nobody's reading, new keys are simply dropped.
 */
static volatile char buffer[BUF_SIZE];
static volatile uint32_t head, tail;

/*
 * runs every time the keyboard has something to say. it works out which key
 * it was and whether it went up or down, keeps track of shift and caps lock,
 * and turns presses into characters for the buffer. extended keys get
 * skipped.
 */
static void keyboard_irq(struct regs *r)
{
    (void)r;
    uint8_t sc = inb(KBD_DATA);

    if (sc == SC_EXTENDED) {
        extended = true;
        return;
    }
    if (extended) {
        extended = false;
        return;
    }

    bool released = sc & SC_RELEASE;
    sc &= ~SC_RELEASE;

    if (sc == SC_LSHIFT || sc == SC_RSHIFT) {
        shift = !released;
        return;
    }
    if (released)
        return;
    if (sc == SC_CAPSLOCK) {
        caps = !caps;
        return;
    }

    char c = shift ? keymap_shift[sc] : keymap[sc];
    if (caps && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
        c ^= 0x20;
    if (!c)
        return;

    uint32_t next = (head + 1) % BUF_SIZE;
    if (next != tail) {
        buffer[head] = c;
        head = next;
    }
}

/*
 * empty out anything the bios left waiting in the keyboard controller, then
 * start listening for keys.
 */
void keyboard_init(void)
{
    while (inb(0x64) & 1)
        inb(KBD_DATA);
    irq_register(1, keyboard_irq);
}

int keyboard_poll(char *c)
{
    if (head == tail)
        return 0;
    *c = buffer[tail];
    tail = (tail + 1) % BUF_SIZE;
    return 1;
}

/*
 * waits for a key, resting the cpu with hlt between interrupts instead of
 * spinning.
 */
char keyboard_getchar(void)
{
    char c;
    while (!keyboard_poll(&c))
        __asm__ volatile("hlt");
    return c;
}
