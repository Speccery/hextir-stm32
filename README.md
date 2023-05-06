# HEXTIr
TI HEX-BUS SD Drive Interface
Copyright (C) 2017-20  Jim Brain

## STM32 version
By Erik Piehl 2023.
Started by creating a fresh STM32 project with STM32CubeIDE. Copied in the source files from the Arduino IDE version (see below). I initially planned to branch the STM32 version, but since I wanted to use the goodies such as SWD debugger of the STM32CubeIDE I recreated the whole project.

The target board is the so-called Black Pill, with a STM32F411CEU chip. This is working progress, not done yet. Something works, it seems to be able to save files to the SD Card. More information to follow.

Update: 
- Now saving and loading from SD card seems to work.
- The STM32 chip implements USB CDC driver i.e. serial port for debug output

### SD card connection
| MCU   | SD card |
|-------|---------|
| PB12  | /CS     |
| PB13  | SCK     |
| PB15  | MOSI    |
| PB14  | MISO    |
| GND   | GND     |
| 3.3V  | 3.3V VCC |

### Connection to TI-74 Dockbbus
| MCU  | Dockbus pin |
|------|-------------|
| N.C  | 1 - PO      |
| N.C. | 2 - PI      |
| PB4  | 3 - D0      |
| PB3  | 4 - D1      |
| PB6  | 5 - D2      |
| PB7  | 6 - D3      |
| PB8  | 7 - HSK     |
| PB9  | 8 - BAV     |
| N.C. | 9 - RESET   |
| GND  | 10 - GND    |

Note: The pins are in the order in which the align directly between the TI-74 Dockbus connector and Black Pill board pins, **EXCEPT** for PB5 which is not 5V tolerant. The corresponding signal is wired to PB3.


## License of the STM32 derived work
Licenses as in the original project, except for the STM32 libraries and generated code which are distributed with their respective licenses. Not sure if all the license text is included in this repository yet.

## Description (original not STM32)
Coupled with an Atmel ATMEGA328 microcontroller (either as a standalone PCB or an 
Arduino UNO, Nano, etc. development board), this firmware implements the functionality
of the various HEXBUS peripherals:

* Secure Digital (SD) based random access disk drive at device # 100
* RS232 port at device # 20
* RS232-based printer port at device #10
* RTC-based clock at device 230

## Implementation
### Arduino Implementation
Configure the Arduino IDE for the specific board in use. Adjust pin mappings in config.h
if the board does not use Arduino UNO mappings.  Once configured, load the 
src.ino file in the src directory, compile and download the object code to the Arduino
system.

Because the GCC C++ compiler and Arduino IDE settings compile the code slightly differently,
the resulting code size is larger and thus not all features can be enabled.  By default, 
only the driver function is enabled in the Arduino build.  You can optionally enable some
of the other peripherals by uncommenting lines in config.h:

```
#elif CONFIG_HARDWARE_VARIANT == 4
/* ---------- Hardware configuration: Old HEXTIr Arduino ---------- */

#define INCLUDE_DRIVE
//#define INCLUDE_CLOCK
//#define INCLUDE_SERIAL
//#define INCLUDE_PRINTER
```

The code has been successfully compiled using Arduino IDE 1.8.19 and 2.0.3.

### Native Implementation
The best code size is obtained by compiling the code natively via the AVR-GCC toolchain.

**To use the PCB design in the PCB directory, run the following make command:**

> make CONFIG=config clean all fuses program

**To use an Arduino UNO with a SD Card shield, run the following make command:**

> make CONFIG=config-arduino clean all fuses program

You may need to adjust the avrdude settings in the respecive config files in the 
main directory.

Native and Arduino builds differ only in the pin mappings.  All features are enabled
in both builds.  If space becomes an issue, you can comment out specific peripherals in config.h:

```
#define INCLUDE_DRIVE
#define INCLUDE_CLOCK
#define INCLUDE_SERIAL
#define INCLUDE_PRINTER
```

## PCB Design Copyright

This project's PCB files are free designs; you can redistribute them 
and/or modify them under the terms of the Creative Commons
Attribution-ShareAlike 4.0 International License.

You should have received a copy of the license along with this
work. If not, see <http://creativecommons.org/licenses/by-sa/4.0/>.

These files are distributed in the hope that they will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
license for more details.

## Firmware Copyright

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; version 2 of the License only.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
