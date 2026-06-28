# Bare Metal UART Serial Communication on STM32F103

A bare metal UART driver written from scratch in C using direct register access, no HAL, no libraries.

# Foreword

Continuing my dive into curiosity for low level stuff, this time I went down to the hardware itself. I wanted to touch the actual metal, no operating system underneath me, just the chip. This is a bare metal UART driver for the STM32F103, no HAL or libraries doing the work for me.
It is currently still June 2026, same as before im using AI to assist me as I reason through what im trying to accomplish, alongside the STM32F103 reference manual (RM0008), which became my bible for this project. Learning came from reading that document.
⚠️SMALL DISCLAIMER same as my other repos, im very forgetful, so you'll see a LOT of comments in the code written in my own words with analogies. Its how I actually understand and remember what each piece does. Its part of my process.

# Challenges where I got really lost

This time i actually felt good about everything, it all made sense except traveling the registers or as i understand it in my mind 'apartment blocks' and having to memorize so many concepts (by name) was a bit disorienting. I often told the AI "check the page of the manual where it speaks about where things live and how to access resources on the board.

## Notes

Enables peripheral clocks through RCC
Configures GPIO pins PA2 (TX) and PA3 (RX) for UART
Sets up USART2 with a 9600 baud rate
Sends a message over serial, byte by byte

## How it works

Wake up the GPIOA and USART2 peripherals by enabling their clocks in RCC
Configure PA2 as a push pull output (talker) and PA3 as floating input (listener)
Set the baud rate in USART2_BRR (8MHz / 9600 = 833)
Enable the transmitter, receiver, and the USART itself in USART2_CR1
Feed the message to the transmitter one byte at a time, waiting for the TXE flag between each

## Build and flash

This is bare metal so it needs a startup file and linker script in addition to main.c. You can either let a tool like STM32CubeIDE generate those automatically, or write them yourself. Build with the ARM toolchain (arm-none-eabi-gcc) and flash with st-flash, OpenOCD, or STM32CubeProgrammer. Full details in the comments at the top of the code.

## Lessons Learned

Every peripheral starts asleep, you have to enable its clock (the heartbeat) in RCC before it responds to anything
The wipe and rewrite pattern: AND with a NOT mask to 'wipe' (as i call it) specific bits, then OR in your new value, so you never disturb bits you dont own when you write new ones.
TX pin must be push pull (actively drives the line), RX pin must be floating input (just listens).
You never touch the TX pin directly, you feed bytes to the USART data register and the hardware serializes them.

## Status
Complete for transmit. Planned additions: receive handling, interrupt driven UART. Maybe writing my own startup file and linker script from scratch?
It sends the message one way for now. Two way comes if/when i add the receive side later.
