#!/bin/bash

# Make all files required for supporting Generation 7 Electronics 
# in the Arduino IDE.
#
# Copyright (c) Markus "Traumflug" Hitter 2011

FLEURY_DIR="stk500v2bootloader"
DIST_DIR="Gen7 Arduino IDE Support"
IDE_DIR="${DIST_DIR}/Gen7"
BOOTLOADERS_DIR="${IDE_DIR}/bootloaders/Gen7"

# Reset to a clean state.
rm -rf "${DIST_DIR}"
rm -f "${DIST_DIR}.zip"
mkdir -p "${DIST_DIR}"
mkdir -p "${IDE_DIR}"
mkdir -p "${BOOTLOADERS_DIR}"
cp INSTALL.txt "${DIST_DIR}"
cp -r "Gen7-dist"/* "${IDE_DIR}"

# Build all required variants of bootloaders.
(cd "${FLEURY_DIR}" && make MCU=atmega644 F_CPU=16000000)
mv "${FLEURY_DIR}/stk500boot.hex" "${BOOTLOADERS_DIR}/bootloader-644-16MHz.hex"
(cd "${FLEURY_DIR}" && make clean)

(cd "${FLEURY_DIR}" && make MCU=atmega644 F_CPU=20000000)
mv "${FLEURY_DIR}/stk500boot.hex" "${BOOTLOADERS_DIR}/bootloader-644-20MHz.hex"
(cd "${FLEURY_DIR}" && make clean)

(cd "${FLEURY_DIR}" && make MCU=atmega644p F_CPU=16000000)
mv "${FLEURY_DIR}/stk500boot.hex" "${BOOTLOADERS_DIR}/bootloader-644P-16MHz.hex"
(cd "${FLEURY_DIR}" && make clean)

(cd "${FLEURY_DIR}" && make MCU=atmega644p F_CPU=20000000)
mv "${FLEURY_DIR}/stk500boot.hex" "${BOOTLOADERS_DIR}/bootloader-644P-20MHz.hex"
(cd "${FLEURY_DIR}" && make clean)

(cd "${FLEURY_DIR}" && make MCU=atmega1284p F_CPU=16000000)
mv "${FLEURY_DIR}/stk500boot.hex" "${BOOTLOADERS_DIR}/bootloader-1284P-16MHz.hex"
(cd "${FLEURY_DIR}" && make clean)

(cd "${FLEURY_DIR}" && make MCU=atmega1284p F_CPU=20000000)
mv "${FLEURY_DIR}/stk500boot.hex" "${BOOTLOADERS_DIR}/bootloader-1284P-20MHz.hex"
(cd "${FLEURY_DIR}" && make clean)

# Package and clean up.
zip -r9 "${DIST_DIR}.zip" "${DIST_DIR}"
rm -rf "${DIST_DIR}"


#  This script is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either
#  version 3 of the License, or (at your option) any later version.
#
#  This library is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#  General Public License for more details.


