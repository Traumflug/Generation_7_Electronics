/*****************************************************************************
Title:     STK500v2 compatible bootloader
Author:    Peter Fleury <pfleury@gmx.ch>   http://jump.to/fleury
File:      $Id: stk500boot.c,v 1.15 2008/05/25 09:09:30 peter Exp $
Compiler:  avr-gcc 4.x / avr-libc 1.4.x
Hardware:  All AVRs with bootloader support, tested with ATmega8
License:   GNU General Public License

DESCRIPTION:
    This program allows an AVR with bootloader capabilities to
    read/write its own Flash/EEprom. To enter Programming mode
    an input pin is checked. If this pin is pulled low, programming mode
    is entered. If not, normal execution is done from $0000
    "reset" vector in Application area.
    Size < 500 words, fits into a 512 word bootloader section
    when compiled with avr-gcc 4.1

USAGE:
    - Set AVR MCU type and clock-frequency (F_CPU) in the Makefile.
    - Set baud rate below (AVRISP only works with 115200 bps)
    - compile/link the bootloader with the supplied Makefile
    - program the "Boot Flash section size" (BOOTSZ fuses),
      for boot-size 512 words:  program BOOTSZ1
    - enable the BOOT Reset Vector (program BOOTRST)
    - Upload the hex file to the AVR using any ISP programmer
    - Program Boot Lock Mode 3 (program BootLock 11 and BootLock 12 lock bits)
    - Reset your AVR while keeping PROG_PIN pulled low
    - Start AVRISP Programmer (AVRStudio/Tools/Program AVR)
    - AVRISP will detect the bootloader
    - Program your application FLASH file and optional EEPROM file using AVRISP

Note:
    Erasing the device without flashing, through AVRISP GUI button "Erase Device"
    is not implemented, due to AVRStudio limitations.
    Flash is always erased before programming.

    Normally the bootloader accepts further commands after programming.
    The bootloader exits and starts applicaton code after programming
    when ENABLE_LEAVE_BOOTLADER is defined.
    Use Auto Programming mode to programm both flash and eeprom,
    otherwise bootloader will exit after flash programming.

    AVRdude:
    Please uncomment #define REMOVE_CMD_SPI_MULTI when using AVRdude.
    Comment #define REMOVE_PROGRAM_LOCK_BIT_SUPPORT to reduce code size
    Read Fuse Bits and Read/Write Lock Bits is not supported

NOTES:
    Based on Atmel Application Note AVR109 - Self-programming
    Based on Atmel Application Note AVR068 - STK500v2 Protocol

LICENSE:
    Copyright (C) 2006 Peter Fleury

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

*****************************************************************************/
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/boot.h>
#include <avr/pgmspace.h>
#include "command.h"

/*
 * Uncomment the following lines to save code space
 */
#define REMOVE_PROGRAM_LOCK_BIT_SUPPORT  // disable program lock bits
#define REMOVE_BOOTLOADER_LED            // no LED to show active bootloader
//#define REMOVE_PROG_PIN_PULLUP           // disable internal pullup, use external
//#define REMOVE_CMD_SPI_MULTI             // disable processing of SPI_MULTI commands

/*
 *  Uncomment to leave bootloader and jump to application after programming.
 */
#define ENABLE_LEAVE_BOOTLADER

/*
 * Uncomment to always wait a few seconds for the programmer before
 * jumping to the application. This makes PROG_PIN and the hardware
 * connected to it obsolete, at the cost of startup time.
 */
#define ALWAYS_WAIT_FOR_PROGRAMMER
#define PROGRAMMER_WAIT_SECONDS 3.0      // how many seconds to wait
#define CMD_PROGRAMMER_TIMEOUT  0x2F     // make sure this doesn't clash
                                         // with the ones in commands.h

/*
 * Pin "PROG_PIN" on port "PROG_PORT" has to be pulled low
 * (active low) to start the bootloader
 * uncomment #define REMOVE_PROG_PIN_PULLUP if using an external pullup
 */
#define PROG_PORT  PORTD
#define PROG_DDR   DDRD
#define PROG_IN    PIND
#define PROG_PIN   PIND2

/*
 * Active-low LED on pin "PROGLED_PIN" on port "PROGLED_PORT"
 * indicates that bootloader is active
 */
#define PROGLED_PORT PORTB
#define PROGLED_DDR  DDRB
#define PROGLED_PIN  PINB1

/*
 * define CPU frequency in Mhz here if not defined in Makefile
 */
#ifndef F_CPU
#define F_CPU 7372800UL
#endif

/*
 * define which UART channel will be used, if device with two UARTs is used
 */
//#define USE_USART1        // undefined means use USART0


/*
 * UART Baudrate, AVRStudio AVRISP only accepts 115200 bps
 */
#define BAUDRATE 115200


/*
 *  Enable (1) or disable (0) USART double speed operation
 */
#define UART_BAUDRATE_DOUBLE_SPEED 0


/*
 * HW and SW version, reported to AVRISP, must match version of AVRStudio
 */
#define CONFIG_PARAM_BUILD_NUMBER_LOW   0
#define CONFIG_PARAM_BUILD_NUMBER_HIGH  0
#define CONFIG_PARAM_HW_VER             0x0F
#define CONFIG_PARAM_SW_MAJOR           2
#define CONFIG_PARAM_SW_MINOR           0x0A



/*
 * Calculate the address where the bootloader starts from FLASHEND and BOOTSIZE
 * (adjust BOOTSIZE below and BOOTLOADER_ADDRESS in Makefile if you want to change the size of the bootloader)
 */
#define APP_END  (BOOTLOADER_ADDRESS - 1)



/*
 * Signature bytes are not available in avr-gcc io_xxx.h
 */
#if defined (__AVR_ATmega8__)
    #define SIGNATURE_BYTES 0x1E9307
#elif defined (__AVR_ATmega16__)
    #define SIGNATURE_BYTES 0x1E9403
#elif defined (__AVR_ATmega32__)
    #define SIGNATURE_BYTES 0x1E9502
#elif defined (__AVR_ATmega8515__)
    #define SIGNATURE_BYTES 0x1E9306
#elif defined (__AVR_ATmega8535__)
    #define SIGNATURE_BYTES 0x1E9308
#elif defined (__AVR_ATmega88__)
    #define SIGNATURE_BYTES 0x1E930A
#elif defined (__AVR_ATmega128__)
    #define SIGNATURE_BYTES 0x1E9702
#elif defined (__AVR_ATmega162__)
    #define SIGNATURE_BYTES 0x1E9404
#elif defined (__AVR_ATmega168__)
    #define SIGNATURE_BYTES 0x1E9406
#elif defined (__AVR_ATmega644__)
    #define SIGNATURE_BYTES 0x1E9609
#elif defined (__AVR_ATmega644P__)
    #define SIGNATURE_BYTES 0x1E960A
#elif defined (__AVR_ATmega1280__)
    #define SIGNATURE_BYTES 0x1E9703
#elif defined (__AVR_ATmega1284P__)
    #define SIGNATURE_BYTES 0x1E9705
#elif defined (__AVR_ATmega2560__)
    #define SIGNATURE_BYTES 0x1E9801
#elif defined (__AVR_ATmega2561__)
    #define SIGNATURE_BYTES 0x1e9802
#elif defined (__AVR_AT90CAN32__)
    #define SIGNATURE_BYTES 0x1E9581
#elif defined (__AVR_AT90CAN64__)
    #define SIGNATURE_BYTES 0x1E9681
#elif defined (__AVR_AT90CAN128__)
    #define SIGNATURE_BYTES 0x1E9781
#else
    #error "no signature definition for MCU available"
#endif


/*
 *  Defines for the various USART registers
 */
#if  defined(__AVR_ATmega8__)     || defined(__AVR_ATmega16__)    || \
     defined(__AVR_ATmega32__)    || defined(__AVR_ATmega8515__)  || \
     defined(__AVR_ATmega8535__)
/*
 * ATMega with one USART
 */
#define UART_BAUD_RATE_LOW       UBRRL
#define UART_STATUS_REG          UCSRA
#define UART_CONTROL_REG         UCSRB
#define UART_ENABLE_TRANSMITTER  TXEN
#define UART_ENABLE_RECEIVER     RXEN
#define UART_TRANSMIT_COMPLETE   TXC
#define UART_RECEIVE_COMPLETE    RXC
#define UART_DATA_REG            UDR
#define UART_DOUBLE_SPEED        U2X

#elif  defined(__AVR_ATmega64__)    || defined(__AVR_ATmega128__)   || \
       defined(__AVR_ATmega162__)   || defined(__AVR_ATmega168__)   || \
       defined(__AVR_ATmega644__)   || defined(__AVR_ATmega644P__)  || \
       defined(__AVR_ATmega1280__)  || defined(__AVR_ATmega1284P__) || \
       defined(__AVR_ATmega2560__)  || defined(__AVR_ATmega2561__)  || \
       defined(__AVR_AT90CAN32__)   || defined(__AVR_AT90CAN64__)   || \
       defined(__AVR_AT90CAN128__)
/*
 *  ATMega with two USART, select USART for bootloader using USE_USART1 define
 */
#ifndef USE_USART1

#define UART_BAUD_RATE_LOW       UBRR0L
#ifdef UBRR0H
#define UART_BAUD_RATE_HIGH      UBRR0H
#endif
#define UART_STATUS_REG          UCSR0A
#define UART_CONTROL_REG         UCSR0B
#define UART_ENABLE_TRANSMITTER  TXEN0
#define UART_ENABLE_RECEIVER     RXEN0
#define UART_TRANSMIT_COMPLETE   TXC0
#define UART_RECEIVE_COMPLETE    RXC0
#define UART_DATA_REG            UDR0
#define UART_DOUBLE_SPEED        U2X0

#else

#define UART_BAUD_RATE_LOW       UBRR1L
#ifdef UBRR1H
#define UART_BAUD_RATE_HIGH      UBRR1H
#endif
#define UART_STATUS_REG          UCSR1A
#define UART_CONTROL_REG         UCSR1B
#define UART_ENABLE_TRANSMITTER  TXEN1
#define UART_ENABLE_RECEIVER     RXEN1
#define UART_TRANSMIT_COMPLETE   TXC1
#define UART_RECEIVE_COMPLETE    RXC1
#define UART_DATA_REG            UDR1
#define UART_DOUBLE_SPEED        U2X1

#endif

#else
    #error "no UART definition for MCU available"
#endif


/*
 * Macros to map the new ATmega88/168 EEPROM bits
 */
#ifdef EEMPE
#define EEMWE EEMPE
#define EEWE  EEPE
#endif


/*
 * Macro to calculate UBBR from XTAL and baudrate
 */
#if UART_BAUDRATE_DOUBLE_SPEED
#define UART_BAUD_SELECT(baudRate,xtalCpu) (((float)(xtalCpu))/(((float)(baudRate))*8.0)-1.0+0.5)
#else
#define UART_BAUD_SELECT(baudRate,xtalCpu) (((float)(xtalCpu))/(((float)(baudRate))*16.0)-1.0+0.5)
#endif


/*
 * States used in the receive state machine
 */
typedef enum {
    ST_START = 0,
    ST_GET_SEQ_NUM,
    ST_MSG_SIZE_1,
    ST_MSG_SIZE_2,
    ST_GET_TOKEN,
    ST_GET_DATA,
    ST_GET_CHECK,
    ST_PROCESS
} parseState_t;


/*
 * use 16bit address variable for ATmegas with <= 64K flash
 */
#if defined(RAMPZ)
typedef uint32_t address_t;
#else
typedef uint16_t address_t;
#endif


/*
 * function prototypes
 */
static void sendchar(char c);
static unsigned char recchar(void);


/*
 * since this bootloader is not linked against the avr-gcc crt1 functions,
 * to reduce the code size, we need to provide our own initialization
 */
void __jumpMain     (void) __attribute__ ((naked)) __attribute__ ((section (".init9")));

void __jumpMain(void)
{
    asm volatile ( ".set __stack, %0" :: "i" (RAMEND) );

    /* init stack here, bug WinAVR 20071221 does not init stack based on __stack */
    asm volatile ("ldi r24,%0":: "M" (RAMEND & 0xFF));
    asm volatile ("ldi r25,%0":: "M" (RAMEND >> 8));
    asm volatile ("out __SP_H__,r25" ::);
    asm volatile ("out __SP_L__,r24" ::);

    asm volatile ( "clr __zero_reg__" );                       // GCC depends on register r1 set to 0
    asm volatile ( "out %0, __zero_reg__" :: "I" (_SFR_IO_ADDR(SREG)) );  // set SREG to 0
#if ! defined(REMOVE_PROG_PIN_PULLUP) || defined(ALWAYS_WAIT_FOR_PROGRAMMER)
    PROG_PORT |= (1<<PROG_PIN);                                // Enable internal pullup
#endif
    asm volatile ( "rjmp main");                               // jump to main()
}


/*
 * send single byte to USART, wait until transmission is completed
 */
static void sendchar(char c)
{
    UART_DATA_REG = c;                                         // prepare transmission
    while (!(UART_STATUS_REG & (1 << UART_TRANSMIT_COMPLETE)));// wait until byte sent
    UART_STATUS_REG |= (1 << UART_TRANSMIT_COMPLETE);          // delete TXCflag
}

/*
 * Read single byte from USART, block if no data available
 */
static unsigned char recchar(void)
{
#ifdef ALWAYS_WAIT_FOR_PROGRAMMER
    // this factor 11. was found experimental
    uint32_t timeout = ( (uint32_t)((float)F_CPU * PROGRAMMER_WAIT_SECONDS / 11.) );
#endif

    while(!(UART_STATUS_REG & (1 << UART_RECEIVE_COMPLETE)))   // wait for data
    {
#ifdef ALWAYS_WAIT_FOR_PROGRAMMER
        if (--timeout == 0)
        {
            return CMD_PROGRAMMER_TIMEOUT;
        }
#endif
    }
    return UART_DATA_REG;
}


int main(void) __attribute__ ((OS_main));
int main(void)
{
    address_t       address = 0;
    parseState_t    msgParseState;
    unsigned int    i = 0;
    unsigned char   checksum = 0;
    unsigned char   seqNum = 0;
    unsigned int    msgLength = 0;
    unsigned char   msgBuffer[285];
    unsigned char   c, *p;
    unsigned char   isLeave = 0;


    /*
     * Branch to bootloader or application code ?
     */
#ifndef ALWAYS_WAIT_FOR_PROGRAMMER
    if(!(PROG_IN & (1<<PROG_PIN)))
    {
#endif
#ifndef REMOVE_BOOTLOADER_LED
        /* PROG_PIN pulled low, indicate with LED that bootloader is active */
        PROGLED_DDR  |= (1<<PROGLED_PIN);
        PROGLED_PORT &= ~(1<<PROGLED_PIN);
#endif
        /*
         * Init UART
         * set baudrate and enable USART receiver and transmiter without interrupts
         */
#if UART_BAUDRATE_DOUBLE_SPEED
        UART_STATUS_REG   |=  (1 <<UART_DOUBLE_SPEED);
#endif

#ifdef UART_BAUD_RATE_HIGH
        UART_BAUD_RATE_HIGH = 0;
#endif
        UART_BAUD_RATE_LOW = UART_BAUD_SELECT(BAUDRATE,F_CPU);
        UART_CONTROL_REG   = (1 << UART_ENABLE_RECEIVER) | (1 << UART_ENABLE_TRANSMITTER);


        /* main loop */
        while ( ! isLeave )
        {
            /*
             * Collect received bytes to a complete message
             */
            msgParseState = ST_START;
            while ( msgParseState != ST_PROCESS )
            {
                c = recchar();
                switch (msgParseState)
                {
                case ST_START:
                    if( c == MESSAGE_START )
                    {
                        msgParseState = ST_GET_SEQ_NUM;
                        checksum = MESSAGE_START^0;
                    }
#ifdef ALWAYS_WAIT_FOR_PROGRAMMER
                    else if ( c == CMD_PROGRAMMER_TIMEOUT )
                    {
                        msgBuffer[0] = CMD_PROGRAMMER_TIMEOUT;
                        msgParseState = ST_PROCESS;
                    }
#endif
                    break;

                case ST_GET_SEQ_NUM:
                    if ( (c == 1) || (c == seqNum) )
                    {
                        seqNum = c;
                        msgParseState = ST_MSG_SIZE_1;
                        checksum ^= c;
                    }
                    else
                    {
                        msgParseState = ST_START;
                    }
                    break;

                case ST_MSG_SIZE_1:
                    msgLength = (unsigned int)c<<8;
                    msgParseState = ST_MSG_SIZE_2;
                    checksum ^= c;
                    break;

                case ST_MSG_SIZE_2:
                    msgLength |= c;
                    msgParseState = ST_GET_TOKEN;
                    checksum ^= c;
                    break;

                case ST_GET_TOKEN:
                    if ( c == TOKEN )
                    {
                        msgParseState = ST_GET_DATA;
                        checksum ^= c;
                        i = 0;
                    }
                    else
                    {
                        msgParseState = ST_START;
                    }
                    break;

                case ST_GET_DATA:
                    msgBuffer[i++] = c;
                    checksum ^= c;
                    if ( i == msgLength )
                    {
                        msgParseState = ST_GET_CHECK;
                    }
                    break;

                case ST_GET_CHECK:
                    if( c == checksum )
                    {
                        msgParseState = ST_PROCESS;
                    }
                    else
                    {
                        msgParseState = ST_START;
                    }
                    break;
                case ST_PROCESS: // silence a compiler warning
                    break;
                }//switch
            }//while(msgParseState)

            /*
             * Now process the STK500 commands, see Atmel Appnote AVR068
             */

            switch (msgBuffer[0])
            {
#ifndef REMOVE_CMD_SPI_MULTI
            case CMD_SPI_MULTI:
                {
                    unsigned char answerByte = 0;

                    // only Read Signature Bytes implemented, return dummy value for other instructions
                    if ( msgBuffer[4]== 0x30 )
                    {
                        unsigned char signatureIndex = msgBuffer[6];

                        if ( signatureIndex == 0 )
                            answerByte = (SIGNATURE_BYTES >>16) & 0x000000FF;
                        else if ( signatureIndex == 1 )
                            answerByte = (SIGNATURE_BYTES >> 8) & 0x000000FF;
                        else
                            answerByte = SIGNATURE_BYTES & 0x000000FF;
                    }
                    msgLength = 7;
                    msgBuffer[1] = STATUS_CMD_OK;
                    msgBuffer[2] = 0;
                    msgBuffer[3] = msgBuffer[4];  // Instruction Byte 1
                    msgBuffer[4] = msgBuffer[5];  // Instruction Byte 2
                    msgBuffer[5] = answerByte;
                    msgBuffer[6] = STATUS_CMD_OK;
                }
                break;
#endif
            case CMD_SIGN_ON:
                msgLength = 11;
                msgBuffer[1]  = STATUS_CMD_OK;
                msgBuffer[2]  = 8;
                msgBuffer[3]  = 'A';
                msgBuffer[4]  = 'V';
                msgBuffer[5]  = 'R';
                msgBuffer[6]  = 'I';
                msgBuffer[7]  = 'S';
                msgBuffer[8]  = 'P';
                msgBuffer[9]  = '_';
                msgBuffer[10] = '2';
                break;

            case CMD_GET_PARAMETER:
                {
                    unsigned char value;

                    switch(msgBuffer[1])
                    {
                    case PARAM_BUILD_NUMBER_LOW:
                        value = CONFIG_PARAM_BUILD_NUMBER_LOW;
                        break;
                    case PARAM_BUILD_NUMBER_HIGH:
                        value = CONFIG_PARAM_BUILD_NUMBER_HIGH;
                        break;
                    case PARAM_HW_VER:
                        value = CONFIG_PARAM_HW_VER;
                        break;
                    case PARAM_SW_MAJOR:
                        value = CONFIG_PARAM_SW_MAJOR;
                        break;
                    case PARAM_SW_MINOR:
                        value = CONFIG_PARAM_SW_MINOR;
                        break;
                    default:
                        value = 0;
                        break;
                    }
                    msgLength = 3;
                    msgBuffer[1] = STATUS_CMD_OK;
                    msgBuffer[2] = value;
                }
                break;

            case CMD_LEAVE_PROGMODE_ISP:
#ifdef ENABLE_LEAVE_BOOTLADER
                isLeave = 1;
#endif
            case CMD_ENTER_PROGMODE_ISP:
            case CMD_SET_PARAMETER:
                msgLength = 2;
                msgBuffer[1] = STATUS_CMD_OK;
                break;

            case CMD_READ_SIGNATURE_ISP:
                {
                    unsigned char signatureIndex = msgBuffer[4];
                    unsigned char signature;

                    if ( signatureIndex == 0 )
                        signature = (SIGNATURE_BYTES >>16) & 0x000000FF;
                    else if ( signatureIndex == 1 )
                        signature = (SIGNATURE_BYTES >> 8) & 0x000000FF;
                    else
                        signature = SIGNATURE_BYTES & 0x000000FF;

                    msgLength = 4;
                    msgBuffer[1] = STATUS_CMD_OK;
                    msgBuffer[2] = signature;
                    msgBuffer[3] = STATUS_CMD_OK;
                }
                break;

            case CMD_READ_LOCK_ISP:
                msgLength = 4;
                msgBuffer[1] = STATUS_CMD_OK;
                msgBuffer[2] = boot_lock_fuse_bits_get( GET_LOCK_BITS );
                msgBuffer[3] = STATUS_CMD_OK;
                break;

            case CMD_READ_FUSE_ISP:
                {
                    unsigned char fuseBits;

                    if ( msgBuffer[2] == 0x50 )
                    {
                        if ( msgBuffer[3] == 0x08 )
                            fuseBits = boot_lock_fuse_bits_get( GET_EXTENDED_FUSE_BITS );
                        else
                            fuseBits = boot_lock_fuse_bits_get( GET_LOW_FUSE_BITS );
                    }
                    else
                    {
                        fuseBits = boot_lock_fuse_bits_get( GET_HIGH_FUSE_BITS );
                    }
                    msgLength = 4;
                    msgBuffer[1] = STATUS_CMD_OK;
                    msgBuffer[2] = fuseBits;
                    msgBuffer[3] = STATUS_CMD_OK;
                }
                break;

#ifndef REMOVE_PROGRAM_LOCK_BIT_SUPPORT
            case CMD_PROGRAM_LOCK_ISP:
                {
                    unsigned char lockBits = msgBuffer[4];

                    lockBits = (~lockBits) & 0x3C;  // mask BLBxx bits
                    boot_lock_bits_set(lockBits);   // and program it
                    boot_spm_busy_wait();

                    msgLength = 3;
                    msgBuffer[1] = STATUS_CMD_OK;
                    msgBuffer[2] = STATUS_CMD_OK;
                }
                break;
#endif
            case CMD_CHIP_ERASE_ISP:
                /*
                 * Ignore that and erase pages as/when the replacement is
                 * received. This strategy assumes packages to be sent in
                 * memory page sized and page aligned chunks, but saves
                 * quite a bit of upload time.
                 */
                msgLength = 2;
                msgBuffer[1] = STATUS_CMD_OK;
                break;

            case CMD_LOAD_ADDRESS:
#if defined(RAMPZ)
                address = ( ((address_t)(msgBuffer[1])<<24)|((address_t)(msgBuffer[2])<<16)|((address_t)(msgBuffer[3])<<8)|(msgBuffer[4]) )<<1;
#else
                address = ( ((msgBuffer[3])<<8)|(msgBuffer[4]) )<<1;  //convert word to byte address
#endif
                msgLength = 2;
                msgBuffer[1] = STATUS_CMD_OK;
                break;

            case CMD_PROGRAM_FLASH_ISP:
            case CMD_PROGRAM_EEPROM_ISP:
                {
                    unsigned int  size = (((unsigned int)msgBuffer[1])<<8) | msgBuffer[2];
                    unsigned char *p = msgBuffer+10;
                    unsigned int  data;
                    unsigned char highByte, lowByte;
                    address_t     tempaddress = address;

                    if ( msgBuffer[0] == CMD_PROGRAM_FLASH_ISP )
                    {
                        // rewrite only in main section (bootloader protection)
                        if  ( address < APP_END )
                        {
                            boot_page_erase(address); // Perform page erase
                            boot_spm_busy_wait();     // Wait until the memory is erased.

                            /* Write FLASH */
                            do {
                                lowByte   = *p++;
                                highByte  = *p++;

                                data =  (highByte << 8) | lowByte;
                                boot_page_fill(address,data);

                                address = address + 2; // Select next word in memory
                                size -= 2;
                            } while(size); // Loop until all bytes written

                            boot_page_write(tempaddress);
                            boot_spm_busy_wait();
                            boot_rww_enable(); // Re-enable the RWW section
                        }
                    }
                    else
                    {
                        /* write EEPROM */
                        do {
                            EEARL = address;            // Setup EEPROM address
                            EEARH = (address >> 8);
                            address++;                  // Select next EEPROM byte

                            EEDR= *p++;                 // get byte from buffer
                            EECR |= (1<<EEMWE);         // Write data into EEPROM
                            EECR |= (1<<EEWE);

                            while (EECR & (1<<EEWE));   // Wait for write operation to finish
                            size--;                     // Decrease number of bytes to write
                        } while(size);                  // Loop until all bytes written
                    }
                    msgLength = 2;
                    msgBuffer[1] = STATUS_CMD_OK;
                }
                break;

            case CMD_READ_FLASH_ISP:
            case CMD_READ_EEPROM_ISP:
                {
                    unsigned int  size = (((unsigned int)msgBuffer[1])<<8) | msgBuffer[2];
                    unsigned char *p = msgBuffer+1;
                    msgLength = size+3;

                    *p++ = STATUS_CMD_OK;
                    if (msgBuffer[0] == CMD_READ_FLASH_ISP )
                    {
                        unsigned int data;

                        // Read FLASH
                        do {
#if defined(RAMPZ)
                            data = pgm_read_word_far(address);
#else
                            data = pgm_read_word_near(address);
#endif
                            *p++ = (unsigned char)data;         //LSB
                            *p++ = (unsigned char)(data >> 8);  //MSB
                            address    += 2;     // Select next word in memory
                            size -= 2;
                        }while (size);
                    }
                    else
                    {
                        /* Read EEPROM */
                        do {
                            EEARL = address;            // Setup EEPROM address
                            EEARH = ((address >> 8));
                            address++;                  // Select next EEPROM byte
                            EECR |= (1<<EERE);          // Read EEPROM
                            *p++ = EEDR;                // Send EEPROM data
                            size--;
                        }while(size);
                    }
                    *p++ = STATUS_CMD_OK;
                }
                break;
#ifdef ALWAYS_WAIT_FOR_PROGRAMMER
            case CMD_PROGRAMMER_TIMEOUT:
                isLeave = 2;
                break;
#endif

            default:
                msgLength = 2;
                msgBuffer[1] = STATUS_CMD_FAILED;
                break;
            }

            /*
             * Now send answer message back
             */
            if ( isLeave < 2 ) {
                sendchar(MESSAGE_START);
                checksum = MESSAGE_START^0;

                sendchar(seqNum);
                checksum ^= seqNum;

                c = ((msgLength>>8)&0xFF);
                sendchar(c);
                checksum ^= c;

                c = msgLength&0x00FF;
                sendchar(c);
                checksum ^= c;

                sendchar(TOKEN);
                checksum ^= TOKEN;

                p = msgBuffer;
                while ( msgLength )
                {
                   c = *p++;
                   sendchar(c);
                   checksum ^=c;
                   msgLength--;
                }
                sendchar(checksum);
                seqNum++;
            }
        } // while ( ! isLeave )

#ifndef REMOVE_BOOTLOADER_LED
        PROGLED_DDR  &= ~(1<<PROGLED_PIN);   // set to default
#endif
#ifndef ALWAYS_WAIT_FOR_PROGRAMMER
    }
#endif

    /*
     * Now leave bootloader
     */
#if ! defined(REMOVE_PROG_PIN_PULLUP) || defined(ALWAYS_WAIT_FOR_PROGRAMMER)
    PROG_PORT &= ~(1<<PROG_PIN);    // set to default
#endif
    boot_rww_enable();              // enable application section

    // Jump to Reset vector in Application Section
    // (clear register, push this register to the stack twice = adress 0x0000/words, and return to this address)
    asm volatile (
        "clr r1" "\n\t"
        "push r1" "\n\t"
        "push r1" "\n\t"
        "ret"     "\n\t"
    ::);

     /*
     * Never return to stop GCC to generate exit return code
     * Actually we will never reach this point, but the compiler doesn't
     * understand this
     */
    for(;;);
}
