

                                    CDC-SPI


    This is the Readme file to firmware-only CDC driver for Atmel AVR
    microcontrollers. For more information please visit
    http://www.recursion.jp/avrcdc/


SUMMARY
=======
    The CDC-SPI performs the CDC (Communication Device Class) connection over
    low-speed USB. It provides the SPI interface through virtual COM port.
    The CDC-SPI is developed by Osamu Tamura.


SPECIFICATION
=============
        master/slave:   master mode only

    CDC-SPI for ATtiny45/85
        clock mode:     0
        clock speed:    250kHz
        slave select:   0/1 bits (ISP/HVSP)

    Internal RC Oscillator is calibrated at startup time. It may be unstable 
    after a long time operation.

    Although the CDC is supported by Windows 2000/XP/Vista/7, Mac OS 9.1/X,
    and Linux 2.4, low-speed bulk transfer is not allowed by the USB standard.
    Use CDC-SPI at your own risk.


USAGE
=====
    Slave Selectors are controlled by SendBreak(/SS0) and DTR(/SS1) signals.
    Use EscapeCommFunction() in Windows.

    Send/Receive binary(8 bit) data stream to/from the COM port.

    [Windows 2000/XP/Vista/7]
    Download "avrcdc_inf.zip" and read the instruction carefully.
 
    [Mac OS X]
    You'll see the device /dev/cu.usbmodem***.

    [Linux]
    The device will be /dev/ttyACM*.
    Linux 2.6 does not accept low-speed CDC without patching the kernel.


DEVELOPMENT
===========
    Build your circuit and write firmware (spimega*.hex/spitiny*.hex) into it.
    R1:1K5 means 1.5K ohms.

    This firmware has been developed on AVR Studio 4.18 and WinAVR 20090313.
    If you couldn't invoke the project from spi*.aps, create new GCC project
    named "spi***" under "cdcspi.****-**-**/" without creating initial file. 
    Select each "default/Makefile" at "Configuration Options" menu.

    There are several options you can configure in Makefile.

    MCU       Select MCU type.   
    F_CPU     Select clock. 16.5MHz is the internal RC oscillator.
              3.3V Vcc may not be enough for the higher clock operation.
    AVRDUDE*  Select your programming tool and it's flags.
              AVRDUDEFLAGSFAST is used for normal operations,
              AVRDUDEFLAGSSLOW is used for operations typically applied to
              factory fresh ATtinys.

    Rebuild all the codes after modifying Makefile.

    Fuse bits
                          ext  H-L                    clock(MHz)
        ATtiny45/85        FF 4E-F1 (RSTDISBL=0) ***   16.5 (PLLx2)

	SPIEN=0, WDTON=0, BOD:1.8-2.7V,
	*** High-voltage Serial programming has to be used to change fuses
            to perform further programming.

    The code size of CDC-SPI is 2-3KB, and 128B RAM is required at least.


USING CDC-SPI FOR FREE
======================
    The CDC-SPI is published under an Open Source compliant license.
    See the file "License.txt" for details.

    You may use this driver in a form as it is. However, if you want to
    distribute a system with your vendor name, modify these files and recompile
    them;
        1. Vendor String in usbconfig.h
        2. COMPANY and MFGNAME strings in avrcdc.inf/lowcdc.inf 



    Osamu Tamura @ Recursion Co., Ltd.
    http://www.recursion.jp/avrcdc/
    6 February 2010

