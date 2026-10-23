#include <console.h>
#include <cpu.h>
#include <heap.h>
#include <http.h>
#include <io.h>
#include <net.h>
#include <string.h>

#define KERNEL_NAME    "Craft Kernel"
#define KERNEL_VERSION "0.1"
#define RELEASE        "Craft Release 1"

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/*
 * just the start of the multiboot info the bootloader hands us. mem_lower and
 * mem_upper are how many kib of ram there are below and above 1 mib; they're
 * only filled in if bit 0 of flags is set.
 */
struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
};

extern char kernel_end[];

static uint32_t mem_upper_kb;

/*
 * the shell: read a line, split off the first word, and look it up in the
 * command table. everything else on the line gets passed along as the
 * arguments.
 */

#define LINE_MAX 128

/*
 * reads a line from the keyboard into buf, echoing it as it's typed, until
 * enter is pressed. only printable characters go in, and backspace rubs out
 * the last one.
 */
static void read_line(char *buf, int max)
{
    int len = 0;
    for (;;) {
        char c = keyboard_getchar();
        if (c == '\n') {
            console_putc('\n');
            buf[len] = '\0';
            return;
        }
        if (c == '\b') {
            if (len > 0) {
                len--;
                console_putc('\b');
            }
        } else if (c >= ' ' && len < max - 1) {
            buf[len++] = c;
            console_putc(c);
        }
    }
}

static void cmd_help(const char *args)
{
    (void)args;
    kprintf("Commands:\n"
            "  help     show this list\n"
            "  clear    clear the screen\n"
            "  echo X   print X\n"
            "  uptime   time since boot\n"
            "  mem      memory info\n"
            "  crash    divide by zero to test the exception handler\n"
            "  reboot   restart the machine\n"
            "network:\n"
            "  ifconfig     the network card and address\n"
            "  ping HOST    send four pings\n"
            "  host NAME    look a name up in dns\n"
            "  http URL     fetch an http:// page and show it\n");
}

/* ---- network commands ---- */

static void cmd_ifconfig(const char *args)
{
    (void)args;
    char buf[512];
    net_describe(buf, sizeof(buf));
    kprintf("%s", buf);
}

/*
 * ping: looks the host up, then sends four pings, two seconds apart at most,
 * and prints how long each reply took (or why there wasn't one).
 */
static void cmd_ping(const char *args)
{
    if (!*args) {
        kprintf("usage: ping HOST\n");
        return;
    }
    uint32_t ip;
    int err = net_resolve(args, &ip);
    if (err < 0) {
        kprintf("ping: %s: %s\n", args, net_error(err));
        return;
    }
    char addr[16];
    net_format_ip(ip, addr);
    kprintf("PING %s (%s)\n", args, addr);
    for (int seq = 1; seq <= 4; seq++) {
        int t = net_ping(ip, seq, 2 * timer_hz());
        if (t < 0)
            kprintf("  seq %d: %s\n", seq, net_error(t));
        else
            kprintf("  seq %d: reply from %s in %u ms\n", seq, addr, (uint32_t)t * 1000 / timer_hz());
    }
}

static void cmd_host(const char *args)
{
    if (!*args) {
        kprintf("usage: host NAME\n");
        return;
    }
    uint32_t ip;
    int err = net_resolve(args, &ip);
    if (err < 0) {
        kprintf("host: %s: %s\n", args, net_error(err));
        return;
    }
    char addr[16];
    net_format_ip(ip, addr);
    kprintf("%s has address %s\n", args, addr);
}

static void progress(const char *msg)
{
    console_set_color(VGA_DARK_GREY, VGA_BLACK);
    kprintf("%s\n", msg);
    console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
}

/*
 * fetches a page and shows the start of it as text. there's no html here, so
 * tags come out just as they are. "example.com" means http://example.com/.
 */
#define HTTP_SHOW 1600

/*
 * http: fetches the url (adding http:// if it's missing), prints the status,
 * size and type, then the first part of the body, with anything that isn't
 * printable left out, and says how much more there was.
 */
static void cmd_http(const char *args)
{
    if (!*args) {
        kprintf("usage: http URL   (for example: http example.com)\n");
        return;
    }
    char url[URL_MAX];
    if (strstr(args, "://"))
        strlcpy(url, args, sizeof(url));
    else
        ksnprintf(url, sizeof(url), "http://%s", args);
    struct http_result res;
    int err = http_get(url, &res, progress);
    if (err < 0) {
        kprintf("http: %s\n", res.error);
        return;
    }
    console_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("HTTP %d, %d bytes%s%s, from %s\n\n", res.status, res.len, res.content_type[0] ? " of " : "",
            res.content_type, res.final_url);
    console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    int shown = res.len < HTTP_SHOW ? res.len : HTTP_SHOW;
    for (int i = 0; i < shown; i++) {
        char c = res.body[i];
        if (c == '\n' || c == '\t' || (c >= ' ' && c < 127))
            console_putc(c == '\t' ? ' ' : c);
    }
    if (res.len > shown)
        kprintf("\n... and %d more bytes\n", res.len - shown);
    else
        kprintf("\n");
    http_free(&res);
}

static void cmd_clear(const char *args)
{
    (void)args;
    console_clear();
}

static void cmd_echo(const char *args)
{
    kprintf("%s\n", args);
}

static void cmd_uptime(const char *args)
{
    (void)args;
    uint32_t t = timer_ticks();
    uint32_t hz = timer_hz();
    kprintf("up %u.%02u seconds (%u ticks at %u Hz)\n", t / hz, (t % hz) * 100 / hz, t, hz);
}

static void cmd_mem(const char *args)
{
    (void)args;
    kprintf("upper memory: %u KiB (%u MiB)\n", mem_upper_kb, mem_upper_kb / 1024);
    kprintf("kernel image: 0x00100000 - %p (%u KiB)\n", kernel_end,
            ((uint32_t)kernel_end - 0x100000) / 1024);
}

/*
 * gcc turns a plain `1 / x` into a comparison that never faults, so the
 * divide instruction gets written out by hand.
 */
static void cmd_crash(const char *args)
{
    (void)args;
    __asm__ volatile("xor %%ecx, %%ecx; div %%ecx" : : : "eax", "ecx", "edx");
}

/*
 * pulsing the cpu's reset line through the keyboard controller is the oldest
 * reboot trick on the pc, and it still works everywhere.
 */
static void cmd_reboot(const char *args)
{
    (void)args;
    kprintf("Rebooting...\n");
    while (inb(0x64) & 2)
        ;
    outb(0x64, 0xFE);
    halt_forever();
}

static const struct {
    const char *name;
    void (*fn)(const char *args);
} commands[] = {
    { "help", cmd_help },     { "clear", cmd_clear },   { "echo", cmd_echo },
    { "uptime", cmd_uptime }, { "mem", cmd_mem },       { "crash", cmd_crash },
    { "reboot", cmd_reboot }, { "ifconfig", cmd_ifconfig }, { "ping", cmd_ping },
    { "host", cmd_host },     { "http", cmd_http },
};

/*
 * runs one line: skips the spaces at the start, splits off the first word as
 * the command, and calls it with the rest of the line. anything not in the
 * table gets an "unknown command".
 */
static void run_command(char *line)
{
    while (*line == ' ')
        line++;
    if (!*line)
        return;

    char *args = line;
    while (*args && *args != ' ')
        args++;
    if (*args)
        *args++ = '\0';

    for (size_t i = 0; i < sizeof(commands) / sizeof(commands[0]); i++) {
        if (strcmp(line, commands[i].name) == 0) {
            commands[i].fn(args);
            return;
        }
    }
    kprintf("unknown command: %s (try 'help')\n", line);
}

static void shell(void)
{
    char line[LINE_MAX];
    for (;;) {
        console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        kprintf("> ");
        console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        read_line(line, sizeof(line));
        run_command(line);
    }
}

static void ok(const char *what)
{
    console_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("[ OK ] ");
    console_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf("%s\n", what);
}

/*
 * called from the assembly entry point. it brings up the cpu tables and the
 * drivers one at a time, switches interrupts on, and drops into the shell.
 */
void kernel_main(uint32_t magic, struct multiboot_info *mbi)
{
    console_init();

    console_set_color(VGA_LIGHT_CYAN, VGA_BLACK);
    kprintf("%s %s (%s)\n\n", KERNEL_NAME, KERNEL_VERSION, RELEASE);
    console_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        panic("not loaded by a Multiboot bootloader");
    if (mbi->flags & 1)
        mem_upper_kb = mbi->mem_upper;

    gdt_init();
    ok("GDT loaded");
    idt_init();
    ok("IDT loaded");
    pic_init();
    ok("PIC remapped to vectors 32-47");
    timer_init(100);
    ok("PIT timer at 100 Hz");
    keyboard_init();
    ok("PS/2 keyboard");

    __asm__ volatile("sti");
    ok("Interrupts enabled");
    heap_init(mem_upper_kb);
    ok("Heap");

    /*
     * the network card, then an address from qemu's (or the network's) dhcp
     * server
     */
    if (net_init()) {
        ok("RTL8139 network card");
        if (net_dhcp()) {
            char ip[16];
            net_format_ip(net.ip, ip);
            char msg[48];
            ksnprintf(msg, sizeof(msg), "DHCP: address %s", ip);
            ok(msg);
        } else {
            kprintf("[FAIL] DHCP: no answer, so no network\n");
        }
    } else {
        kprintf("[ -- ] No network card (QEMU needs -nic user,model=rtl8139)\n");
    }

    kprintf("\n%u MiB of memory available. Type 'help' for commands.\n\n", mem_upper_kb / 1024);
    shell();
}
