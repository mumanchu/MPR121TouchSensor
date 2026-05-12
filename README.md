# MPR121 Capacitive Touch Sensor Library, from $${\color{green}mumanchu}$$

## *** PRELIMINARY ***

The MPR121 Capacitive Touch Sensor chip provides 12 capacitive touch sensor input channels (0..11), and one 'proximity sensor' channel (12).
The upper 8 channels (4..11) can be configured as GPIOs if you don't need them as touch sensors.

> [!CAUTION]
> **THIS IS A 3.3V CHIP! IT CANNOT BE USED WITH 5V MCUs! BANG!** (see Disclaimer) \
> Well, you could power it with 3.3V and use logic level translators for SCL and SDA so they NEVER see a 5V signal. \
> SDA needs a fast bi-directional level translator.

It has a 400kHz I2C interface, with addresses 0x5A, 0x5B, 0x5C and 0x5D, according to the ADDR pin connection.

Many of the recent MPR121 chips are probably "clones" because NXP stopped manufacturing this chip in 2019, so they are 'not recommended for new designs'. The chips I am using were about the cheapest on 
AliExpress, with no 'genuine chip' guarantee, but they all seem to work very well. NXP probably sold the chip design to another company.

Full full details, read the commented source code in `MPR121TouchSensor.h`. A complete example sketch will be added soon.

## Avantages of this library

There are a couple other Arduino MPR121 libraries out there. One is too small, and the other is too big.

- Fully configured GPIO handling (other libraries have none)
- Additional checks (when using `#ifdef DEBUG`) to ensure your clone chip is working properly
- `dumpRegisters(Stream*)` method so you can see what's happening
- Uses shadow registers to optimize read-modify-write
- Uses a `TwoWire*` pointer, so more than one I2C channel can be used on your board
- Has useful comments and easy-to-understand code

## Installation and Use

Download the ZIP file from github via the green "\< \> Code" button. \
Unzip it and put the `MPR121TouchSensor.h` and `MumanchuDebug.h` files into your Sketch source directory. \
Add this code to the start of your Sketch:
```
#include "MumanchuDebug.h"
//TODO copy/paste the code here which is indicated in MumancheDebug.h
#include "MPR121TouchSensor.h"
MPR121TouchSensor touchSensor;		// this is the touch sensor object
```
The `MumanchuDebug.h` file defines the `ASSERT()` and `LOGERROR()` macros which are used when `#define DEBUG` is used to generate additional code during development and testing. \
Copy/paste the indicated code from `MumanchuDebug.h` at the start of your Sketch, so the LogError() function is available (in one place). \
Alternatively you can just define empty macros like this, if you don't need them: \
```
#define ASSERT(b)
#define LOGERROR(s)
```
Create your MPR121 object and call the code to initialize the sensor. Here's an example... \
```
void setup()
{
	...
	// I2C, SDA and SCL pins
	Wire.begin((uint32_t)PB_9, (uint32_t)PB_8);
	Wire.setClock(400000);
	Wire.setTimeout(100);

	// configure the chip to use the IRQ pin, which you can connect to an MCU input
	if (!touchSensor.begin(&Wire, 0x5a, MPR121_IRQ_PIN)) {
		Serial.println("touchSensor.begin() failed");
		while (1) yield();
	}
	// configure 4 touch sensor channels
	if (!touchSensor.configureChannels(4)) {
		Serial.println("touchSensor.configureChannel() failed");
		while (1) yield();
	}
	if (!touchSensor.setThresholds(20, 10)) {
		Serial.println("touchSensor.setThresholds() failed");
		while (1) yield();
	}
	/*this is done by configureChannels()
	if (!touchSensor.setDebounceCounts(1, 1)) {
		Serial.println("touchSensor.setDebounceCounts() failed");
		while (1) yield();
	}
	*/
	if (!touchSensor.enableAutoConfiguration()) {
		Serial.println("touchSensor.setAutoConfiguration() failed");
		while (1) yield();
	}
	// configure 7 General Purpose I/Os (GPIOS) just as an example
	// channels 0..3 are touch sensors, 4..11 are GPIOs
	if (!touchSensor.setFirstGpio(4)) {
		Serial.println("touchSensor.setFirstGpio() failed");
		while (1) yield();
	}
	// configure all the GPIOs as inputs with pull-ups
	for (uint channel = 4; channel <= 11; ++channel) {
		if (!touchSensor.setGpioMode(channel, touchSensor.MPR121_INPUT_PULLUP)) {
			Serial.println("touchSensor.setGpioMode() failed");
			while (1) yield();
		}
	}
	// start the touch sensors
	if (!touchSensor.setRunMode()) {
		Serial.println("touchSensor.setRunMode() failed");
		while (1) yield();
	}
	...
}
```
Now you can poll the touch sensors and check the inputs, in `loop()`:
```
void loop()
{
	// has a touch sensor been touched?
	uint touchedState;
	if (touchSensor.sensorTouched(&touchedState)) {
		// touch sensor touched
		Serial.printf("\rtouchedState=%08x\r\n", touchedState);
		Serial.flush();
	}
	
	// read the GPIO inputs
	byte data;
	touchSensor.gpioRead(&data);
	Serial.printf("gpio=%02X\n\r", data);
	Serial.flush();

	if (data & MPR121_GPIO_MASK(4)) {
		// input channel 4 active

	}
	...
}
```
The chip is very versatile and fully programmable with 128 internal registers! This is why the library looks a bit complicated at first, and why each part is configured separately - if you need it.

> [!NOTE]
> For full details, read the descriptions of each method in the code. These also contain data sheet page references, `p16` etc.


## Stop Mode and Run Mode

'Stop mode' is when the chip is not looking at the sensors. 
'Run mode' is when the sensors are being scanned and touch/release can be detected. 
After calling `begin()`, the chip is in 'stop mode' and it will not register any touches. 
The chip can only be configured when in 'stop mode' because most registers cannot be written unless it's in stop mode. 
Once configuration is complete, call `setRunMode()` to start the scanning.

## Using the IRQ Pin - Poll It!
Instead of using this pin to generate an interrupt, this pin can be connected to an input (INPUT_PULLUP) and polled for touch/release changes. It is set low on a touch/release change, and set high when
the Touch Status registers (0x00..0x01) are read. Pass the IRQ pin number to `begin()`, and call `sensorTouched()` to poll it. \

Why not use an interrupt? \
The problem is that you can't read the sensor state in an interrupt handler because I2C methods can't be called from interrupt handlers. Instead, the interrupt handler must set a 'touchStateChanged' flag, and poll that 
flag from `loop()`. But that is the same polling the IRQ pin as in input. 

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

Resistance is useless!

You must use a capacitor.

