// ============================================================
// HOW TO BUILD AND FLASH THIS
// ============================================================
// This is bare metal C. To turn it into something the chip runs
// you need two extra files besides this one:
//   1. a startup file (startup.s) written in ARM assembly
//      it sets up the stack and the vector table then calls main
//   2. a linker script (linker.ld) that tells the build where
//      flash and RAM live and what goes where in memory
//
// You have two options for those two files:
//
// OPTION A, let a tool handle it
//   - STM32CubeIDE or CubeMX auto generates both files for you
//     just drop this main.c in and hit run, you never see them
//   - this is the easy path, the files exist but stay hidden
//
// OPTION B, do it yourself from scratch
//   - write startup.s by hand in ARM assembly
//   - write linker.ld by hand in linker script syntax
//   - this is the real bare metal flex, harder but you own everything
//
// TOOLS THAT DO THE WORK
//   - arm-none-eabi-gcc   the ARM compiler, the main one everyone uses
//   - arm-none-eabi-as    the assembler, processes startup.s
//   - arm-none-eabi-ld    the linker, processes linker.ld
//                         (gcc usually calls as and ld for you)
//
// TOOLS THAT FLASH IT ONTO THE CHIP
//   - st-flash            simplest, command line, made for ST boards
//   - OpenOCD             more powerful, also lets you debug
//   - STM32CubeProgrammer ST official tool, GUI or command line
//   - STM32CubeIDE        full IDE that wraps all of the above
// ============================================================

#include <stdint.h>

// We are writing straight to the chips registers
// No libraries, no HAL, just us and the metal
// Every part of the chip lives at a fixed address in memory
// Writing to that address is like flipping a real switch on the hardware
// The chip does not understand C, it only understands voltage
// high or low, 1 or 0, yes or no, on or off

// --- BASE ADDRESSES ---
// Every part of the chip has a home address, like a house number
// RCC is the clock part, it hands out the heartbeat to everyone else
// GPIOA is the pin part, our two pins PA2 and PA3 live here
// USART2 is the engine that actually does the talking and listening

#define RCC_BASE    0x40021000
#define GPIOA_BASE  0x40010800
#define USART2_BASE 0x40004400

// --- RCC REGISTERS ---
// The heartbeat is a fast pulse, high low high low, over and over
// it is the beat everything moves to, like a metronome
// every part of the chip starts asleep, no heartbeat reaching it, frozen
// RCC is the part that wakes others up by sending them the heartbeat
// no heartbeat means the part is asleep and ignores everything we say
// APB2ENR is the switch that wakes up the GPIOA pin part
// APB1ENR is the switch that wakes up the USART2 engine part

#define RCC_APB2ENR  (*(volatile uint32_t *)(RCC_BASE   + 0x18))
#define RCC_APB1ENR  (*(volatile uint32_t *)(RCC_BASE   + 0x1C))

// --- GPIOA REGISTERS ---
// CRL is the setup sheet for pins 0 through 7
// each pin gets 4 boxes on the sheet, 2 for mode and 2 for type
// 8 pins times 4 boxes equals 32 boxes, fills the whole sheet exactly
// PA2 is our talker, it sends by pushing voltage high or low, 1 or 0
// PA3 is our listener, it waits for voltage coming in from outside

#define GPIOA_CRL    (*(volatile uint32_t *)(GPIOA_BASE + 0x00))

// --- USART2 REGISTERS ---
// SR is the status sheet, the engine uses it to raise its hand
// it says either yes i am ready or no i am still busy
// DR is the drop box, we drop one letter in and the engine sends it
// the engine takes that letter and turns it into a stream of
// high low pulses on the talker pin, 1s and 0s going out as voltage
// BRR sets the speed of those pulses, called the baud rate
// both sides have to agree on the speed or the listener hears nonsense
// CR1 is the on off panel for the whole engine

#define USART2_SR    (*(volatile uint32_t *)(USART2_BASE + 0x00))
#define USART2_DR    (*(volatile uint32_t *)(USART2_BASE + 0x04))
#define USART2_BRR   (*(volatile uint32_t *)(USART2_BASE + 0x08))
#define USART2_CR1   (*(volatile uint32_t *)(USART2_BASE + 0x0C))

// --- TRANSMIT FUNCTION ---
// goes through the message one letter at a time
// before each letter it checks the status sheet, bit 7, called TXE
// TXE is the engine raising its hand saying yes i am ready for the next one
// if it is 1 the engine is ready, if it is 0 it is still busy so we wait
// once ready we drop the letter into the drop box
// the engine grabs it and pushes it out as high low pulses on the talker pin
// we never touch the pin ourselves, we just keep feeding the engine letters

void uart_send(const char *str) {
    while (*str) {
        // wait right here until the engine raises its hand, TXE goes to 1
        while (!(USART2_SR & (1 << 7)));

        // drop the current letter into the drop box
        // the engine turns it into 1s and 0s on the talker pin
        // *str++ hands over the current letter then steps to the next one
        USART2_DR = *str++;
    }
}

int main(void) {

    // =====================
    // STEP 1: WAKE THEM UP
    // =====================
    // GPIOA and USART2 are asleep right now, no heartbeat reaching them
    // we flip switches in RCC to send the heartbeat to each one
    // we use OR so we only flip our switches and leave the rest alone
    // if we wrote the whole thing directly we might flip switches we dont own

    // send the heartbeat to GPIOA (switch 2) and AFIO (switch 0)
    // AFIO is what lets the pins act as UART pins instead of plain pins
    RCC_APB2ENR |= (1 << 2) | (1 << 0);

    // send the heartbeat to USART2 (switch 17)
    RCC_APB1ENR |= (1 << 17);


    // ==========================
    // STEP 2: SET UP THE PINS
    // ==========================
    // PA2 is the talker, it actively pushes voltage high or low to send 1s and 0s
    // pushing both ways like that is called push pull
    // PA3 is the listener, it just floats and reads whatever voltage comes in
    // floating means it is not pushing anything, only listening
    //
    // here is the important part
    // the setup sheet has 32 boxes and we only own boxes 8 through 15
    // all the other boxes belong to other pins
    // if we just write our value straight in, we erase the whole sheet
    // that would wipe every other pins setup, which is very bad
    //
    // so we use a NOT mask as a specifier
    // it lets us say wipe only our boxes and leave everyone elses alone
    // 0xF is 1111, we slide it over to sit right on top of our boxes
    // NOT flips it, our boxes turn to 0s and every other box turns to 1s
    // we AND it with the sheet, our boxes get wiped, every other box survives
    //
    // now our boxes are clean and empty so we OR in our new values
    // 0xB is 1011, the talker setup for PA2
    // 0x4 is 0100, the listener setup for PA3

    // wipe only our boxes, PA2 boxes (8-11) and PA3 boxes (12-15)
    GPIOA_CRL &= ~((0xF << 8) | (0xF << 12));

    // write our setup into the now clean boxes, talker and listener
    GPIOA_CRL |= (0xB << 8) | (0x4 << 12);


    // ========================
    // STEP 3: SET UP THE ENGINE
    // ========================
    // the pins are ready, now we set up the engine that drives them
    // first the speed, then we flip the engine on

    // set the speed to 9600
    // the heartbeat runs at 8 million beats a second by default
    // 8000000 divided by 9600 equals 833
    // this is how fast the engine pushes 1s and 0s out the talker pin
    // if the other side is set to a different speed it just hears nonsense
    USART2_BRR = 833;

    // flip on the talker side of the engine (switch 3), now it can send
    // flip on the listener side of the engine (switch 2), now it can receive
    // flip on the whole engine (switch 13), the main power switch
    USART2_CR1 |= (1 << 3) | (1 << 2) | (1 << 13);


    // =====================
    // STEP 4: SEND THE MESSAGE
    // =====================
    // hand the message to the engine one letter at a time
    // the engine turns each letter into high low pulses on the talker pin
    // those pulses travel through the ST-Link cable to your screen as text
    // the \r\n at the end just bumps it down to a clean new line

    uart_send("Hello from bare metal!\r\n");

    // the chip cannot run off the end of main or it breaks
    // so we spin here forever, just sitting, doing nothing
    while(1);

    return 0;
}
