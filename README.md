# Craft Kernel 0.1 (Craft Release 1)

- note this README was written with ai because Craft drained me xx - bitetheapple

The first Craft Kernel (0.1, the text-mode kernel from CraftOS 1) with HTTP networking added: a
driver for the RTL8139 network card, a small IPv4 stack, and a plain HTTP client, used from its
little shell. There's no HTML, CSS, JavaScript or HTTPS. The OS for this release is next to it
in `../CraftOS`.

The Craft Kernel is a hobby x86 (32-bit) kernel written from scratch in C with a little assembly.
It boots through Multiboot, so QEMU can load it directly without a disk image.

## What it does

- Sets up its own stack and calls `kernel_main` (`src/boot.S`)
- Flat GDT, IDT with all 32 CPU exceptions, and the 8259 PIC remapped to vectors 32-47
- VGA text console with colors, scrolling and a cursor, mirrored to the COM1 serial port
- PIT timer at 100 Hz and a PS/2 keyboard driver (US layout, shift, caps lock)
- A heap (`kmalloc`, `kfree`, `kzalloc`, `krealloc`) in the memory above the kernel
- Networking: PCI scanning to find the RTL8139 card, its driver, and an IPv4 stack on top
  (ARP, ICMP, UDP, DHCP at boot, DNS and TCP), with an HTTP/1.1 client that follows redirects
  and understands chunked answers
- A small shell:
  - `help`, `clear`, `echo`, `uptime`, `mem`, `crash`, `reboot`
  - `ifconfig`: the card, the address from DHCP, and how much has gone in and out
  - `ping HOST`: four pings (QEMU's gateway, 10.0.2.2, answers them)
  - `host NAME`: a DNS lookup
  - `http URL`: fetches an `http://` page (`http example.com` works too) and shows the start of
    it, as text
- A red register dump and panic screen on any CPU exception

The kernel runs one thing at a time: there's no scheduler yet. So when the network waits for an
answer, the CPU halts until the next timer tick, and the network card's interrupt still handles
frames as they arrive.

## Building

One-time setup, on Ubuntu (or WSL Ubuntu):

```sh
sudo apt update && sudo apt install -y build-essential gcc-multilib qemu-system-x86
```

or on Fedora:

```sh
sudo dnf install -y gcc make binutils glibc-devel.i686 libgcc.i686 qemu-system-x86
```

Then, from this folder:

```sh
make          # builds build/craft-kernel.elf
make run      # boots it in a QEMU window, with the network card
make run-term # boots it inside the terminal (quit: Esc, 2, then type `quit`)
```

`make run` passes `-nic user,model=rtl8139`, which gives the kernel its network card and QEMU's
private network (with a DHCP server, and the host computer at 10.0.2.2). Without it the kernel
still boots, offline.

## Layout

| Path               | What it is                                              |
| ------------------ | ------------------------------------------------------- |
| `src/boot.S`       | Multiboot header and entry point                        |
| `src/interrupts.S` | GDT/IDT loading and the interrupt entry stubs           |
| `src/gdt.c`        | Segment descriptors                                     |
| `src/idt.c`        | Interrupt table, exception handler, IRQ dispatch, panic |
| `src/pic.c`        | 8259 PIC setup                                          |
| `src/timer.c`      | PIT timer, and waiting a number of ticks                |
| `src/keyboard.c`   | PS/2 keyboard driver                                    |
| `src/console.c`    | VGA text and serial output, `kprintf`                   |
| `src/heap.c`       | The kernel heap                                         |
| `src/pci.c`        | PCI bus scanning                                        |
| `src/rtl8139.c`    | RTL8139 network card driver                             |
| `src/net.c`        | Ethernet, ARP, IPv4, ICMP, UDP, DHCP and DNS            |
| `src/tcp.c`        | TCP (client side)                                       |
| `src/http.c`       | URLs and the HTTP client (plain HTTP)                   |
| `src/string.c`     | Memory and string functions                             |
| `src/format.c`     | `ksnprintf`                                             |
| `src/kernel.c`     | `kernel_main` and the shell                             |
| `linker.ld`        | Places the kernel at 1 MiB                              |

## AI Use in Craft.

AI was used in the following:

- commenting - im lazy
- README writing - im also lazy

