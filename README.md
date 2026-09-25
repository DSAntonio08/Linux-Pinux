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

Very early development.

The PCB and components have been ordered, and I’m currently working on the first UART pin configuration. Nothing has been verified on the actual board yet.

The first milestone is to run my own code from microSD and get a recognizable message over UART.

## Planned development

- Check the hardware when it arrives.
- Get the first UART output from my own code.
- Develop the bootloader until it can start Linux.
- Build a minimal user space with my own init program, shell and basic utilities.
- Bring up USB, Ethernet and HDMI step by step.

I’ll document what works, what doesn’t and what I learn along the way.