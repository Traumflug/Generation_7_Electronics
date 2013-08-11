
/*
   This file is copied from USBaspLoader.2010-07-27.zip

   The contents of this file should go either into ../usbconfig.h
   or into main.c. Dissolving this file has been started, but not yet
   completed.

   When this is done, command handling from USBaspLoader's main.c
   should be moved step by step into the main.c here, always ensuring
   the USB stack is kept in a functional state.

   Markus "Traumflug" Hitter, November 2012
*/

/*
General Description:
This file (together with some settings in Makefile) configures the boot loader
according to the hardware.

This file contains (besides the hardware configuration normally found in
usbconfig.h) two functions or macros: bootLoaderInit() and
bootLoaderCondition(). Whether you implement them as macros or as static
inline functions is up to you, decide based on code size and convenience.

bootLoaderInit() is called as one of the first actions after reset. It should
be a minimum initialization of the hardware so that the boot loader condition
can be read. This will usually consist of activating a pull-up resistor for an
external jumper which selects boot loader mode.

bootLoaderCondition() is called immediately after initialization and in each
main loop iteration. If it returns TRUE, the boot loader will be active. If it
returns FALSE, the boot loader jumps to address 0 (the loaded application)
immediately.

For compatibility with Thomas Fischl's avrusbboot, we also support the macro
names BOOTLOADER_INIT and BOOTLOADER_CONDITION for this functionality. If
these macros are defined, the boot loader usees them.
*/

/* ------------------------------------------------------------------------- */
/* ---------------------- feature / code size options ---------------------- */
/* ------------------------------------------------------------------------- */

#define HAVE_EEPROM_PAGED_ACCESS    1
/* If HAVE_EEPROM_PAGED_ACCESS is defined to 1, page mode access to EEPROM is
 * compiled in. Whether page mode or byte mode access is used by AVRDUDE
 * depends on the target device. Page mode is only used if the device supports
 * it, e.g. for the ATMega88, 168 etc. You can save quite a bit of memory by
 * disabling page mode EEPROM access. Costs ~ 138 bytes.
 */
#define HAVE_EEPROM_BYTE_ACCESS     1
/* If HAVE_EEPROM_BYTE_ACCESS is defined to 1, byte mode access to EEPROM is
 * compiled in. Byte mode is only used if the device (as identified by its
 * signature) does not support page mode for EEPROM. It is required for
 * accessing the EEPROM on the ATMega8. Costs ~54 bytes.
 */
#define BOOTLOADER_CAN_EXIT         1
/* If this macro is defined to 1, the boot loader will exit shortly after the
 * programmer closes the connection to the device. Costs ~36 bytes.
 */
#define HAVE_CHIP_ERASE             0
/* If this macro is defined to 1, the boot loader implements the Chip Erase
 * ISP command. Otherwise pages are erased on demand before they are written.
 */
//#define SIGNATURE_BYTES             0x1e, 0x93, 0x07, 0     /* ATMega8 */
/* This macro defines the signature bytes returned by the emulated USBasp to
 * the programmer software. They should match the actual device at least in
 * memory size and features. If you don't define this, values for ATMega8,
 * ATMega88, ATMega168 and ATMega328 are guessed correctly.
 */

/* The following block guesses feature options so that the resulting code
 * should fit into 2k bytes boot block with the given device and clock rate.
 * Activate by passing "-DUSE_AUTOCONFIG=1" to the compiler.
 * This requires gcc 3.4.6 for small enough code size!
 */
#if USE_AUTOCONFIG
#   undef HAVE_EEPROM_PAGED_ACCESS
#   define HAVE_EEPROM_PAGED_ACCESS     (USB_CFG_CLOCK_KHZ >= 16000)
#   undef HAVE_EEPROM_BYTE_ACCESS
#   define HAVE_EEPROM_BYTE_ACCESS      1
#   undef BOOTLOADER_CAN_EXIT
#   define BOOTLOADER_CAN_EXIT          1
#   undef SIGNATURE_BYTES
#endif /* USE_AUTOCONFIG */

/* ------------------------------------------------------------------------- */

/* Example configuration: Port D bit 3 is connected to a jumper which ties
 * this pin to GND if the boot loader is requested. Initialization allows
 * several clock cycles for the input voltage to stabilize before
 * bootLoaderCondition() samples the value.
 * We use a function for bootLoaderInit() for convenience and a macro for
 * bootLoaderCondition() for efficiency.
 */

#ifndef __ASSEMBLER__   /* assembler cannot parse function definitions */

#define JUMPER_BIT  7   /* jumper is connected to this bit in port D, active low */

#ifndef MCUCSR          /* compatibility between ATMega8 and ATMega88 */
#   define MCUCSR   MCUSR
#endif

static inline void  bootLoaderInit(void)
{
    PORTD |= (1 << JUMPER_BIT);     /* activate pull-up */
    if(!(MCUCSR & (1 << EXTRF)))    /* If this was not an external reset, ignore */
        leaveBootloader();
    MCUCSR = 0;                     /* clear all reset flags for next time */
}

static inline void  bootLoaderExit(void)
{
    PORTD = 0;                      /* undo bootLoaderInit() changes */
}

#define bootLoaderCondition()   ((PIND & (1 << JUMPER_BIT)) == 0)

#endif /* __ASSEMBLER__ */

/* ------------------------------------------------------------------------- */

#endif /* __bootloader_h_included__ */
