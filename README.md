# Linux Pinux

A lightweight Linux distribution for my own hardware.

This is my final-year electrical engineering high school project (Matura project). I designed the OlivePi board around the Allwinner T113-S3 with 128 MB of integrated RAM. Now I’m working on the software that will run on it.

I want to understand what happens from the moment the board gets power to the point where I can use it. My goal is to write my own **Linux**  Distro.

There is no fixed application for the finished system. I’m building it to learn about embedded systems, Linux and computer engineering.

## Hardware

The OlivePi board includes:

- Allwinner T113-S3 with 128 MB RAM
- microSD as the main boot medium
- SPI-NOR flash
- UART for debugging
- Four USB-A ports through a USB hub
- Ethernet
- HDMI through an ADV7511
- USB-C for power

## Current state

Early development, the hardware is up and running.

The OlivePi PCB has arrived and is assembled. I measured every supply voltage and they are all correct.

My own SPL (Secondary Program Loader, the first code that runs after the chip's Boot ROM) now runs on the board:

- The Boot ROM loads my image from microSD (eGON.BT0 header from `start.S`, checksum and padding from `tools/mkboot0`).
- UART1 (PG6/PG7, 115200 8N1) prints text to a serial terminal.
- A small GPIO driver for Port G (`gpio_set_func`, `gpio_read`, `gpio_write`) blinks an LED on PG13.

The first milestone, running my own code from microSD and getting a recognizable message over UART, is reached.

## Building

The SPL is built with the Arm GNU toolchain (`arm-none-eabi-gcc`):

```
cd bootloader/spl
make
```

This produces `build/spl-boot.bin`. The Boot ROM reads it from the microSD card at an offset of 8 KiB:

```
sudo dd if=build/spl-boot.bin of=/dev/<sd-card> bs=1024 seek=8
```

## Planned development

- [x] Get the first UART output from my own code.
- [x] Drive GPIOs (blinking LED on PG13).
- [x] Check that the timer really runs at 24 MHz.
- [ ] Set up clocks and PLLs.
- [ ] Initialize DRAM.
- [ ] Load the next bootloader stage from microSD.
- [ ] Develop the bootloader until it can start Linux.
- [ ] Build a minimal user space with my own init program, shell and basic utilities.
- [ ] Bring up USB, Ethernet and HDMI step by step.

I’ll document what works, what doesn’t and what I learn along the way.