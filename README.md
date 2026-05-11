# MPR121 Capacitive Touch Sensor Library, from $${\color{green}mumanchu}$$

*** PRELIMINARY ***

The MPR121 Capacitive Touch Sensor chip provides 12 capacitive touch sensor input channels (0..11), and one 'proximity sensor' channel (12).
The upper 8 channels (4..11) can be configured as GPIOs if you don't need them as touch sensors.

>>> IT IS A 3.3V CHIP! IT CANNOT BE USED WITH 5V MCUs! <<<
(Well, you could power it with 3.3V and use logic level translators for SCL and SDA so they NEVER see a 5V signal. SDA needs a bi-directional translator.)

It has a 400kHz I2C interface, with addresses 0x5A, 0x5B, 0x5C and 0x5D, according to the ADDR pin connection.

Many of the recent MPR121 chips are probably "clones" because NXP stopped manufacturing this chip in 2019, so they are 'not reccomended for new designs'. The chips I am using were about the cheapest on 
AliExpress, with no 'genuine chip' guarantee, but they all seem to work very well. NXP probably sold the chip design to another company.

## Avantages of this library

There are a couple other Arduino MPR121 libraries out there. One is too small, and one is too big.

This library has these advantages:
- fully configured GPIO handling (other libraries have none)
- additional checks (when #ifdef DEBUG) to ensure your clone chip 
  is working properly
- dumpRegisters(Stream*) method so you can see what's happening
- uses shadow registers to optimize read-modify-write
- uses TwoWire* pointer, so more than one I2C channel can be used on your board
- uesful comments and easy-to-understand code

## Stop Mode and Run Mode

'Stop mode' is when the chip is not looking at the sensors. 
'Run mode' is when the sensors are being scanned and touch/release can be detected. 
After calling `begin()`, the chip is in 'stop mode' and it will not register any touches. 
The chip can only be configured when in 'stop mode' because most registers cannot be written unless it's in stop mode. 
Once configuration is complete, call `setRunMode()` to start the scanning.

## Using the IRQ PIN - Poll It!
Instead of using this pin to generate an interrupt, this pin can be connected to an input (INPUT_PULLUP) and polled for touch/release changes. It is set low on a touch/release change, and set high when
the Touch Status registers (0x00..0x01) are read.

Pass the IRQ pin number to begin(), and call sensorTouched() to poll it. 

Why not use an interrupt?
The problem is that you can't read the sensor state in an interrupt handler because I2C methods can't be called from interrupt handlers. Instead, you must just set a 'touchStateChanged' flag, and poll that 
flag from loop(). But that is the same polling the IRQ pin. 

## Data Sheet

All 'pxx' page numbers in the source code relate to this data sheet \
https://www.nxp.com/docs/en/data-sheet/MPR121.pdf

## Application Notes

AN3889, MPR121 Capacitive Sensing Settings \
https://www.nxp.com/docs/en/application-note/AN3889.pdf

AN3891, MPR121 Baseline System \
https://www.nxp.com/docs/en/application-note/AN3891.pdf

AN3893, MPR121 Proximity Detection \
https://www.nxp.com/docs/en/application-note/AN3893.pdf

AN3894, MPR121 GPIO and LED Driver Function \
https://www.nxp.com/docs/en/application-note/AN3894.pdf


# Revision History

| Date       | Version  | Details |
|:---------- |:---------|:----------- |
| 2026.05.11 | 0.0.0	| Preliminary |

<br/>

# Joke of the Week

Resistance is useless! You must use a capacitor.

