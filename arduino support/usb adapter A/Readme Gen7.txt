
This is the software for the ExtensionBoard USB A. For usage and
installation instructions, see

http://reprap.org/wiki/Gen7_ExtensionBoard_USB_A_1.0


The project has been postponed for the following reasons:

 - Running the ATtiny as SPI slave requires either the absence of other
   SPI devices, like an SD card, or disabling of the Reset pin for a
   working Slave Select pin. The latter implies there's a bootloader for
   software updates required, which makes things more complex.

 - Benchmarking the V-USB solution has showed a maximum transfer rate of
   about 7.6 kBytes/second or approxximately an equivalent of 90'000 baud.
   The benchmark matches the numbers found in various forum postings in
   the Internet. Far away from the USB limit of 1'875'000 baud.

   While 90'000 baud are reasonable, using off-the-shelf USB-TTL adapters
   are already faster, so acceptance of this solution is questionable.

 - Allowing a firmware upload to the main microcontroller, the ATmega,
   requires also programming capabilities, which essentially means the
   ATtiny has to be a 3-in-1 device. Yet more complexity.

 - The upcoming ARM microcontrollers - the low end ones don't feature
   an on-chip USB stack - can't be programmed by SPI, but via RS-232-type
   serial only. An ATtiny85 doesn't have sufficient pins for all this
   and an ATtiny2313 requires more parts on the board, still allows
   34800 baud only.

 - A dedicated USB-UART adapter chip with reasonably wide (1.27 mm) pin
   spacing has been found, the Microchip MCP2200.


Status of development

 - The USB stack works and has been tested. The file main.c has an #if'd
   out section to demonstrate a loopback test.

 - Contents in the folder "firmware" and "bootloader" are still mostly
   the same. The plan was to replace bootloader's command handling with
   that of USBaspLoader:

   http://www.obdev.at/products/vusb/usbasploader.html

 - So far no efforts have been done to allow programming the ATmega. The plan
   was to enable the bootloader to forward incoming programming requests via
   SPI to the ATmega. This would avoid duplicating the logic to handle
   incoming commands, but also require some way to decide which device is
   about to be programmed.

   The latter is a solvable solution, e.g. by swapping the target on each
   request. Worst case, avrdude would see the wrong chip signature when
   attempting to send a firmware and abort. A second, identical attempt would
   succeed, then.


Directory contents:

 usbdrv:

   This is the V-USB driver stack. Unmodified extracted from the most recent
   distribution (2012-01-09 as of this writing)

 libs-device:

   Some helper files coming with V-USB, mostly unchanged.

 usbconfig.h:

   USB configuration shared between firmware and bootloader. These two define
   FIRMWARE or BOOTLOADER in their Makefile, so minor differences can be dealt
   with.

 firmware:

   The main application, made for forwarding G-code commands to the ATmega and
   sending resonses back.

 bootloader:

   An V-USB based bootloader, derived from firmware. Should get the command set
   from USBaspLoader.

Notes:

 - As the ATtiny doesn't have a bootloader section like the bigger AVR chips,
   bootloader support is implemented by writing an rjmp to the bootloader code
   at address 0x0000. Special care has to be taken uploading a firmware
   doesn't overwrite this.

   The usual way to take this care is to always rewrite this rjmp on each
   firmware upload, regardless of what avrdude sends. However, I'm not sure
   wether USBaspLoader does so. When continueing this path, please check this.

 - The Makefiles work well. "make" to just compile, "make program" to compile
   and upload the code. The bootloader Makefile has an additional target
   "make fuses" which sets the fuses correctly. So far, all programming
   requires an ISP programmer.


Markus "Traumflug" Hitter, November 2012


