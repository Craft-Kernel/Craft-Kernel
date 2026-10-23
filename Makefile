# build inside wsl or linux. needs gcc (with -m32 support), binutils and
# qemu-system-i386.

CC      := gcc
LD      := ld
QEMU    := qemu-system-i386

CFLAGS  := -m32 -march=i686 -std=gnu11 -O2 -g -Wall -Wextra \
           -ffreestanding -fno-pic -fno-pie -fno-stack-protector \
           -fno-asynchronous-unwind-tables -mgeneral-regs-only \
           -Iinclude
ASFLAGS := -m32 -fno-pic -fno-pie
LDFLAGS := -m elf_i386 -T linker.ld -nostdlib -z noexecstack

SRCS    := $(wildcard src/*.c) $(wildcard src/*.S)
OBJS    := $(patsubst src/%,build/%.o,$(SRCS))
KERNEL  := build/craft-kernel.elf

.PHONY: all run run-term clean

all: $(KERNEL)

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

build/%.c.o: src/%.c $(wildcard include/*.h) | build
	$(CC) $(CFLAGS) -c $< -o $@

build/%.S.o: src/%.S | build
	$(CC) $(ASFLAGS) -c $< -o $@

build:
	mkdir -p build

# opens a qemu window. needs wslg or an x server.
# -nic gives it the rtl8139 network card it drives.
run: $(KERNEL)
	$(QEMU) -kernel $(KERNEL) -serial stdio -nic user,model=rtl8139

# runs inside the terminal instead. quit with esc, 2, then type `quit`.
run-term: $(KERNEL)
	$(QEMU) -kernel $(KERNEL) -display curses -nic user,model=rtl8139

clean:
	rm -rf build
