#pragma once

/////////////////////////////////////////////////////////////////////
// MPR121 NXP Touch Sensor Libray
// Copyright (C) mumanchu and muman.ch, 2026.05.11
// All rights reversed
// see https://github.com/mumanchu
/*
The MPR121 Touch Sensor chip provides 12 capacitive touch sensor 
input channels (0..11), and one 'proximity sensor' channel (12).
The upper 8 channels (4..11) can be configured as GPIOs if you 
don't need them as touch sensors.

IT IS A 3.3V CHIP! IT CANNOT BE USED WITH 5V MCUs!
(Well, you could power it with 3.3V and use logic level translators
for SCL and SDA so they NEVER see a 5V signal. SDA needs a bi-
directional translator.)

It has a 400kHz I2C interface, with addresses 0x5A, 0x5B, 0x5C and 
0x5D, according to the ADDR pin connection.

Many of the recent MPR121 chips are probably "clones" because NXP 
stopped manufacturing this chip in 2019, so they are 'not recommended
for new designs'. The chips I am using were about the cheapest on 
AliExpress, with no 'genuine chip' guarantee, but they all seem to 
work very well. NXP probably sold the chip design to another company.

ADVANTAGES OF THIS LIBRARY
There are several other Arduino MPR121 libraries out there. 
This one has these advantages:
- Fully configured GPIO handling (other libraries have none)
- Additional checks (when #ifdef DEBUG) to ensure your clone chip 
  is working properly
- dumpRegisters(Stream*) method so you can see what's happening
- Uses shadow registers to optimize read-modify-write
- Uses TwoWire* pointer, so more than one I2C channel can be used
- Has useful comments and easy-to-understand code

STOP MODE AND RUN MODE
'Stop mode' is when the chip is not looking at the sensors.
'Run mode' is when the sensors are being scanned and touch/release
can be detected. After calling begin(), the chip is in 'stop mode' 
and it will not register any touches. 
The chip can only be configured when in 'stop mode' because most 
registers cannot be written unless it's in stop mode. 
Once configuration is complete, call setRunMode() to start the 
scanning.

USING THE IRQ PIN - POLL IT!
Instead of using this pin to generate an interrupt, this pin can be 
connected to an input (INPUT_PULLUP) and polled for touch/release
changes. It is set low on a touch/release change, and set high when
the Touch Status registers (0x00..0x01) are read.
Pass the IRQ pin number to begin(), and call sensorTouched() to 
poll it. 
Why not use an interrupt?
The problem is that you can't read the sensor state in an interrupt
handler because I2C methods can't be called from interrupt handlers. 
Instead, you must just set a 'touchStateChanged' flag, and poll that 
flag from loop(). But that is the same polling the IRQ pin.

DATA SHEET
All 'pxx' page numbers relate to this data sheet
https://www.nxp.com/docs/en/data-sheet/MPR121.pdf

APPLICATION NOTES

AN3889, MPR121 Capacitive Sensing Settings
https://www.nxp.com/docs/en/application-note/AN3889.pdf

AN3891, MPR121 Baseline System
https://www.nxp.com/docs/en/application-note/AN3891.pdf

AN3893, MPR121 Proximity Detection
https://www.nxp.com/docs/en/application-note/AN3893.pdf

AN3894, MPR121 GPIO and LED Driver Function
https://www.nxp.com/docs/en/application-note/AN3894.pdf
*/

class MPR121TouchSensor
{
protected:
	// I2C communications stuff
	TwoWire* wire;
	byte i2cAdds;

	// IRQ pin, polled if defined
	byte irqPin;

	// Shadow registers
	byte ecrRegShadow;				// Electrode Configuration Register ECR
	byte gpioDirectionRegShadow;	// GPIO configuration registers
	byte gpioCtl0RegShadow;
	byte gpioCtl1RegShadow;
	byte gpioEnableRegShadow;
	byte ffiBits;					// FFI bits of register 0x5c

	// Touch and proximity sensor channels
	byte numberOfTouchChannels;		// see configureChannels()
	byte proximitySensingMode;		// see enableProximitySensing()
	uint lastTouchedStates;			// for sensorTouched()

	// Bit set if GPIO channel has been configured with setGpioMode()
	// bit     7  6  5 4 3 2 1 0
	// channel 11 10 9 8 7 6 5 4
	byte gpioChannelConfigured;
	// First channel used as GPIO, 0 = no GPIO channels, else 4..11
	byte firstGpioChannel;

public:
	bool begin(TwoWire* twoWire, byte i2cAddress, byte irqPin);
	bool softReset();
	bool configureChannels(byte numberOfTouchChannels);
	bool enableAutoConfiguration();
	bool enableProximitySensing(byte mode);
	bool setThresholds(byte touchThreshold, byte releaseThreshold);
	bool setDebounceCounts(byte releaseCount, byte touchCount);
	bool setRunMode();
	bool setStopMode();
	inline bool inStopMode();

	// Mask for channel bits 0..12, 12 = proximity channel
	#define MPR121_CHANNEL_MASK(channel) (1 << channel)

	bool readTouchStatus(uint* touchStatus, bool* ovcf = NULL);
	bool sensorTouched(uint* touchedStates);
	bool readOutOfRangeStatus(uint* oorStatus, bool* arff = NULL, bool* acff = NULL);

	// GPIO Configuration and Access
	enum MPR121_GPIO_MODE : byte
	{
		MPR121_INPUT = 0,						// Hi-Z input
		MPR121_INPUT_PULLDOWN = 1,				// internal pullup resistor to 3.3V (?Kohms)
		MPR121_INPUT_PULLUP = 2,				// internal pulldown resistor to GND (?Kohms)
		MPR121_OUTPUT = 3,						// on = +3.3V, off = GND
		MPR121_OUTPUT_HIGHSIDE_OPENDRAIN = 4,	// on = +3.3V, off = floating <- DO NOT CONNECT TO GND!
		MPR121_OUTPUT_LOWSIDE_OPENDRAIN = 5		// on = GND,   off = floating <- DO NOT CONNECT TO 3.3V!
	};

	bool setFirstGpio(byte firstGpioChannel);
	bool setGpioMode(byte channel, MPR121_GPIO_MODE mode, bool enable = true);
	bool gpioEnable(byte channel, bool enable = true);
	inline bool gpioRead(byte* data);
	#define MPR121_GPIO_MASK(channel) (1 << (channel - 4))
	inline bool gpioSet(byte channel);
	inline bool gpioClear(byte channel);
	inline bool gpioToggle(byte channel);

	bool writeRegister(byte reg, byte data);
	bool readRegister(byte reg, byte* data, byte nregs = 1);
	bool dumpRegisters(Stream* stream);

protected:
	bool updateGpio(byte reg, byte channel);
};

// Initialise 'Wire' before calling this
// a soft reset is done so all the chip's registers are set to defaults
// call configureChannels() after this to set up the touch sensors
bool MPR121TouchSensor::begin(TwoWire* twoWire, byte i2cAddress, byte irqPin)
{
	ASSERT(twoWire != NULL && i2cAddress >= 0x5a && i2cAddress <= 0x5d);
	ASSERT(twoWire->getHandle() != NULL);
	wire = twoWire;
	i2cAdds = i2cAddress;

	// IRQ pin is polled for touch/release changes, see sensorTouched()
	this->irqPin = irqPin;
	if (irqPin != PNUM_NOT_DEFINED)
		pinMode(irqPin, INPUT_PULLUP);

	return softReset();
}

// Soft Reset 
// This clears all registers, except for the Filter and 
// Global CDC CDT Configuration registers (0x5c, 0x5d)
// 0x5c := 0x10, 0x5d := 0x24
// the chip remains in 'stop mode' after the soft reset
// next, call configureChannels()
bool MPR121TouchSensor::softReset()
{
	// all shadow registers are 0
	ecrRegShadow = 0;
	gpioDirectionRegShadow = 0;
	gpioCtl0RegShadow = 0;
	gpioCtl1RegShadow = 0;
	gpioEnableRegShadow = 0;
	ffiBits = 0;

	// no touch channels or GPIOs configured
	proximitySensingMode = 0;
	numberOfTouchChannels = 0;
	firstGpioChannel = 0;
	gpioChannelConfigured = 0;
	lastTouchedStates = 0;

	// do soft reset
	if (!writeRegister(0x80, 0x63))
		return false;

	#ifdef DEBUG
	// did the reset work? is I2C communications working well?
	// all registers should be 0, except 0x5c=0x10 and 0x5d=0x24
	byte data[0x80];
	if (!readRegister(0, data, sizeof(data)))
		return false;
	uint sum = 0;
	for (uint n = 0; n < sizeof(data); ++n)
		sum += data[n];
	if (sum != (0x10 + 0x24)) {
		LOGERROR("softReset failed");
		return false;
	}
	#endif

	return true;
}

// Configure the sensor channels
// If not all sensors are connected they must not be enabled,
// so set 'numberOfTouchChannels' to the last sensor which is used, 0..11
// Next steps, call these methods if required:
// setThresholds()
// setDebounceCounts()
// enableAutoConfiguration()
// enableProximitySensing()
// setRunMode()
bool MPR121TouchSensor::configureChannels(byte numberOfTouchChannels)
{
	this->numberOfTouchChannels = numberOfTouchChannels;

	// Configure some settings 
	static const byte configSettings[] =
	{
		// 'reg, value' pairs

		// Baseline filtering control, p12 or AN3891 Baseline System
		// max half delta MHD
		0x2b, 0x01,			// MHDR, MHD Rising
		0x2f, 0x01,			// MHDF, MHD Falling
		// noise half delta NHD
		0x2c, 0x01,			// NHDR, NHD Rising
		0x30, 0x01,			// NHDF, NHD Falling
		0x33, 0x01,			// NHDT, NHD Touched
		// noise count limit NCL
		0x2d, 0x01,			// NCLR, NCL Rising
		0x31, 0x01,			// NCLF, NCL Falling
		0x34, 0x01,			// NCLT, NCL Touched
		// filter delay limit FDL
		0x2e, 0x01,			// FDLR, FDL Rising
		0x32, 0x01,			// FDLF, FDL Falling
		0x35, 0x01,			// FDLT, FDL Touched

		// ELEXPROX filtering settings, p12 or AN3891 Baseline System
		// max half delta MHD
		0x36, 0x01,			// MHDPROXR, MHD Rising
		0x3a, 0x01,			// MHDPROXF, MHD Falling
		// noise half delta NHD
		0x37, 0x01,			// NHDPROXR, NHD Rising
		0x3b, 0x01,			// NHDPROXF, NHD Falling
		0x3e, 0x01,			// NHDPROXT, NHD Touched
		// noise count limit NCL
		0x38, 0x01,			// NCLPROXR, NCL Rising
		0x3c, 0x01,			// NCLPROXF, NCL Falling
		0x3f, 0x01,			// NCLPROXT, NCL Touched
		// filter delay limit FDL
		0x39, 0x01,			// FDLPROXR, FDL Rising
		0x3d, 0x01,			// FDLPROXF, FDL Falling
		0x40, 0x01,			// FDLPROXT, FDL Touched

		// Debounce Counts, or use setDebounceCounts(), p13
		//      0rrr0ttt	rrr=release, ttt=touch
		0x5b, 0b00010001,	// release = 1, touch = 1

		// Analog Front End (AFE) configuration 0x5c, p14
		// Bit     76543210
		//         ffcccccc
		//         00010000 <- default value, 6 samples
		// ff      FFI num samples, same as bits 7..6 in auto config reg 0
		// cccccc  CDC charge/discharge current = 16uA (default)
		0x5c, 0b00010000,	// default = 0x10, 6 samples

		// Filter Configuration 0x5d, p14
		// Bit  76543210
		//      tttsseee
		//      00100100 <- default value
		// ttt  CDT charge/discharge time = 0.5us (default)
		// ss   SFI second filter iterations = 4 (default)
		// eee  ESI electrode sample interval = 16ms (default)
		0x5d, 0b00100100,	// default = 0x24, 5us/4/16ms

		// Electrode Configuration Register ECR 0x5e, p16
		// Bit   76543210
		//       ccppeeee
		// cc    CL calibration lock / baseline tracking
		// pp    proximity sensing enable
		// eeee  electrode enable
		0x5e, 0b10000000,	// baseline tracking enabled, 5 bits used

		// NOTES
		// Registers 0x5f..0x72 are set up by auto configuration, 
		//   see enableAutoConfiguration()
		// GPIO registers 0x73..0x7a are set up by setFirstGpio() 
		//   and setGpioMode()
	};

	for (int i = 0; i < sizeof(configSettings); i += 2) {
		byte reg  = configSettings[i];
		byte data = configSettings[i + 1];
		if (!writeRegister(reg, data))
			return false;

		// ECR shadow register, 0x5e
		if (reg == 0x5e)
			ecrRegShadow = data;
		// AFE register 0x5c, save FFI bits for enableAutoConfiguration()
		if (reg == 0x5c)
			ffiBits = data & 0b11000000;
	}

	// see 'next steps' in comment above
	return true;
}

// Enable/disable Auto Configuration, p17
// so it automatically configures the electrode currents and charge times
// This should normally be enabled, so always call this method after
// calling configureChannels()
bool MPR121TouchSensor::enableAutoConfiguration()
{
	// Auto-configure Control Register 0 (0x7b) and 1 (0x7c), p17
	// Reg   0x7b     0x7c
	// Bit   76543210 76543210
	//       ffrrbbae s0000oic
	//       00001011 00000000  <- values we use
	// 
	// ff    FFI, same as FFI bits in register 0x5c, see configureChannels()
	// rr    RETRY, no. of retries before setting OOR, 00=none, 01=2, 10=4, 11=8
	// bb    BVA, same as CL bits in ECR register 0x5e (normally 00)
	// a     ARE, 1 = auto REconfiguration enable, 0 = disable
	// e     ACE, 1 = auto configuration enable, 0 = disable
	// 
	// s     SCTS, skip charge time search, 1 = skip, 0 = both CDTx and CDCx will be searched 
	// o     OORIE, out-of-range interrupt enable, 1 = enable, 0 = disable 
	// i     ARFIE, auto REconfiguration fail interrupt enable, 1 = enable, 0 = disable 
	// c     ACFIE, auto configuration fail interrupt enable, 1 = enable, 0 = disable

	// Auto configuration control register 0, AN3889 p9
	// ffiBits was set by configureChannels()
	// "Normal setup = 0x0B or 0b00001011.This means that FFI is 00, but if the FFI
	// in the AFE Configuration Register is different, it must be changed to match."
	if (!writeRegister(0x7b, 0b00001011 | ffiBits))
		return false;

	// Auto configuration control register 1
	// (change this to enable IRQ on auto configuration fail, p17)
	if (!writeRegister(0x7c, 0b00000000))
		return false;

	// set limit and target values for 3.3V, p18 and p19
	// USL up-side limit
	if (!writeRegister(0x7d, 202))	// USL = ((Vdd - 0.7) / Vdd) * 256, eqn.1
		return false;
	// TL target level
	if (!writeRegister(0x7f, 182))	// TL = USL * 0.9, eqn.2
		return false;
	// LSL low-side limit
	if (!writeRegister(0x7e, 131))	// LSL = USL * 0.65, eqn.3
		return false;

	return true;
}

/*why disable it? if enabled, it should always be enabled
bool MPR121TouchSensor::disableAutoConfiguration()
{
	// to disable auto-configuration, just clear the ARE and ACE bits
	byte b;
	if (!readRegister(0x7b, &b))
		return false;
	return writeRegister(0x7b, b & 0b11111100);
}
*/


// Set the touch and release thresholds for all the channels, p13 
// threshold : 0..255 (0x00..0xff), typically in the range 0x04..0x10
// The thresholds provide hysteresis and prevent noise and jitter.
// The touch threshold must be higher than the release threshold.
// See application note AN3892 for details.
bool MPR121TouchSensor::setThresholds(byte touchThreshold, byte releaseThreshold)
{
	for (byte reg = 0x41; reg <= 0x59; reg += 2) {
		if (!writeRegister(reg, touchThreshold))
			return false;
		if (!writeRegister(reg + 1, releaseThreshold))
			return false;
	}
	return true;
}

// Set the 'debounce counts' for all channels, p13
// All channels share the same touch and release debounce counts.
// Thr touch or release status is only set when the number of
// consecutive states reaches the debounce count. 
// Use this to reduce noise glitches.
bool MPR121TouchSensor::setDebounceCounts(byte releaseCount, byte touchCount)
{
	ASSERT(releaseCount <= 7 && touchCount <= 7);
	// 0rrr0ttt, rrr=release, ttt=touch
	return writeRegister(0x5b, (releaseCount << 4) + touchCount);
}

// Enable/disable Proximity Sensing, p16
// Enables 'channel 12' sensor using ELEPROX_EN bits in the ECR register
// All enabled electrodes are summed to create a single large electrode,
// which covers a larger sensing area.
// See application note AN3893.
// mode : 0 = disabled
//        1 = sensors 0..1 combined (first 2 sensors)
//        2 = sensors 0..3 combined (first 4 sensors, excludes possible GPIOs)
//        3 = sensors 0..11 combined (all sensors)
// NOTE: 'run mode' is set if proximity sensing is enabled, 'stop mode' set
// if it's disabled and all touch sensors are disabled, see inStopMode()
bool MPR121TouchSensor::enableProximitySensing(byte mode)
{
	ASSERT(mode <= 3);
	proximitySensingMode = mode;
	ecrRegShadow &= ~0b00110000;
	ecrRegShadow |= (mode << 4);
	return writeRegister(0x5e, ecrRegShadow);
}

// Electrode Configuration Register (ECR)
//   76353210
//   ccxxeeee
//   cc  = CL, Calibration Lock, baseline tracking
//   xx  = ELEPROX_EN, 0 = proximity detection disabled
//   eee = ELE_EN, 0 = electrode detection disabled

// Run mode, turn on all sensors according to the configuration
// Sensor touches are not detected unless it's in 'run mode'
// return false if 'run mode' was not enabled
bool MPR121TouchSensor::setRunMode()
{
	// preserve Calibration Lock CL bits
	ecrRegShadow &= 0b11000000;
	// enable the configured channels and proximity sensing
	ecrRegShadow |= numberOfTouchChannels;
	ecrRegShadow |= (proximitySensingMode << 4);
	bool inRunMode = (ecrRegShadow & 0b00111111) != 0;
	return writeRegister(0x5e, ecrRegShadow) && inRunMode;
}

// Enter 'stop mode', turn off all channels
// registers can only be written when in 'stop mode'
// (except for ECR and GPIO registers)
bool MPR121TouchSensor::setStopMode()
{
	// preserve Calibration Lock CL bits
	ecrRegShadow &= 0b11000000;
	return writeRegister(0x5e, ecrRegShadow);
}

// Stop mode is when all channels are disabled
bool MPR121TouchSensor::inStopMode()
{
	return (ecrRegShadow & 0b00111111) == 0;
}

// Returns true if one or more sensors have been touched since 
// the last call. If the IRQ pin is defined, it uses the IRQ pin 
// to detect changes. If IRQ is not defined, it polls the Touched
// Status registers 0x00..0x01.
// It only registers new touches, not new releases.
// 'touchedStates' has a bit for each channel.
// Check the state of an individual bit by ANDing with the
// MPR121_CHANNEL_MASK(channel) macro.
// channels 0..11 are the touch sensors
// channel 12 is the proximity sensor
// debouncing/filtering is already done by the chip
bool MPR121TouchSensor::sensorTouched(uint* touchedStates)
{
	// poll the IRQ pin
	if (irqPin != PNUM_NOT_DEFINED) {
		// pin is active low
		if (digitalRead(irqPin))
			return false;
	}
	uint currentTouchedStates;
	if (!readTouchStatus(&currentTouchedStates)) {
		*touchedStates = 0;
		return false;
	}
	uint changedStates = (currentTouchedStates ^ lastTouchedStates);
	uint newStates = changedStates & currentTouchedStates;
	*touchedStates = newStates;
	lastTouchedStates = currentTouchedStates;
	return newStates != 0;
}

// Return the state of the touch status bits, p11
// and optionally return the Over Current Flag OVCF state
// 'touchStatus' has a bit for each channel
// Check the state of an individual bit by ANDing with the
// MPR121_CHANNEL_MASK(channel) macro.
// channels 0..11 are the touch sensors
// channel 12 is the proximity sensor
bool MPR121TouchSensor::readTouchStatus(uint* touchStatus, 
	bool* ovcf /*=NULL*/)
{
	byte data[2];
	bool ok = readRegister(0x00, data, 2);
	*touchStatus = ((uint)(data[1] & 0x1f) << 8) + data[0];
	if (ovcf)
		*ovcf = (data[1] & 0x80) ? true : false;
	return ok;
}

// Return the state of the out-of-range status bits, p19
// and optionally the ARFF (auto REconfiguration fail) and 
// ACFF (auto configuration fail) states.
// Use this to detect if auto configuration has failed.
// 'oorStatus' has a bit for each channel's OOR state
// check the state of an individual bit by ANDing with the
// MPR121_CHANNEL_MASK(channel) macro.
// channels 0..11 are the touch sensors
// channel 12 is the proximity sensor
bool MPR121TouchSensor::readOutOfRangeStatus(uint* oorStatus, 
	bool* arff /*=NULL*/, bool* acff /*=NULL*/)
{
	byte data[2];
	bool ok = readRegister(0x02, data, 2);
	*oorStatus = ((uint)(data[1] & 0x1f) << 8) + data[0];
	if (arff)
		*arff = (data[1] & 0x40) ? true : false;
	if (acff)
		*acff = (data[1] & 0x80) ? true : false;
	return ok;
}


/////////////////////////////////////////////////////////////////////
// GPIO Configuration and Access
// General Purpose Input/Outputs, p19, p20
// 
// Only channels 4..11 can be configured as GPIO channels
// Channels 0..3 (if enabled) are always capacitive touch sensors
// To use channels as GPIOs, define the first GPIO channel (4..11). 
// All channels from and including 'firstGpioChannel' up to channel 11 
// are assigned as GPIOs and cannot be used as touch sensors.
// Each GPIO channel must then be configured by calling setGpioMode()
// for each channel before it can be accessed.
// NOTE! 'firstGpioChannel' must not overlap 'numberOfElectrodes' in the
// configureChannels(byte numberOfElectrodes). 
bool MPR121TouchSensor::setFirstGpio(byte firstGpioChannel)
{
	ASSERT(firstGpioChannel >= 4 && firstGpioChannel <= 11);
	ASSERT(firstGpioChannel >= numberOfTouchChannels);

	this->firstGpioChannel = firstGpioChannel;

	// disable all GPIOs by writing 0 to the Enable Register, all become Hi-Z
	gpioEnableRegShadow = 0;
	if (!writeRegister(0x77, 0))
		return false;

	// update ECR register to set the first GPIO
	ecrRegShadow = (ecrRegShadow & 0b11110000) | firstGpioChannel;
	if (!writeRegister(0x5e, ecrRegShadow))
		return false;

	// each GPIO channel must now be configured by calling setGpioMode()
	gpioChannelConfigured = 0;
	gpioDirectionRegShadow = 0;
	gpioCtl0RegShadow = 0;
	gpioCtl1RegShadow = 0;

	return true;
}

// Configure a GPIO channel, p19
// first call setFirstGpio(), then call this to cofigure each GPIO
bool MPR121TouchSensor::setGpioMode(byte channel, MPR121_GPIO_MODE mode, 
	bool enable /*=true*/)
{
	ASSERT(channel >= 4 && channel <= 11 && channel >= firstGpioChannel);
	ASSERT(mode <= MPR121_OUTPUT_LOWSIDE_OPENDRAIN);

	// register bit for gpio channel
	byte mask = MPR121_GPIO_MASK(channel);

	// direction : 0 = input, 1 = output
	if (mode >= MPR121_OUTPUT)
		gpioDirectionRegShadow |= mask;
	else
		gpioDirectionRegShadow &= ~mask;
	if (!writeRegister(0x76, gpioDirectionRegShadow))
		return false;

	// bits for Control Register 0 and Control Register 1
	gpioCtl0RegShadow &= ~mask;
	gpioCtl1RegShadow &= ~mask;
	switch(mode) {
	case MPR121_INPUT:
		break;
	case MPR121_INPUT_PULLDOWN:
		gpioCtl0RegShadow |= mask;
		break;
	case MPR121_INPUT_PULLUP:
		gpioCtl0RegShadow |= mask;
		gpioCtl1RegShadow |= mask;
		break;
	case MPR121_OUTPUT:
		break;
	case MPR121_OUTPUT_HIGHSIDE_OPENDRAIN:
		gpioCtl0RegShadow |= mask;
		gpioCtl1RegShadow |= mask;
		break;
	case MPR121_OUTPUT_LOWSIDE_OPENDRAIN:
		gpioCtl0RegShadow |= mask;
		break;
	default:
		ASSERT(0);
	}
	if (!writeRegister(0x73, gpioCtl0RegShadow))
		return false;
	if (!writeRegister(0x74, gpioCtl1RegShadow))
		return false;

	// enable it, else channel is Hi-Z until gpioEnable() is called
	if (enable) {
		gpioEnableRegShadow |= mask;
		if (!writeRegister(0x77, gpioEnableRegShadow))
			return false;
	}

	// channel has been configured
	gpioChannelConfigured |= mask;

	return true;
}

// Enable/disable a GPIO channel, goes to Hi-Z when disabled, p20
bool MPR121TouchSensor::gpioEnable(byte channel, bool enable/*=true*/)
{
	ASSERT((channel >= 4 && channel <= 11) && (channel >= firstGpioChannel));
	byte mask = MPR121_GPIO_MASK(channel);
	if (enable)
		gpioEnableRegShadow |= mask;
	else
		gpioEnableRegShadow &= ~mask;
	return writeRegister(0x77, gpioEnableRegShadow);
}

// Read the state of all GPIOs, channels 4..11, p20
//   data bit   7  6  5  4  3  2  1  0
//   channel   11 10  9  8  7  6  5  4 
// Check the state of an individual input by ANDing with the 
// MPR121_GPIO_MASK(channel) macro.
bool MPR121TouchSensor::gpioRead(byte* data)
{
	return readRegister(0x75, data);
}

// Set, clear or toggle a single channel that has been 
// configured as an Output, p20
bool MPR121TouchSensor::gpioSet(byte channel)
{
	return updateGpio(0x78, channel);
}
bool MPR121TouchSensor::gpioClear(byte channel)
{
	return updateGpio(0x79, channel);
}
bool MPR121TouchSensor::gpioToggle(byte channel)
{
	return updateGpio(0x7a, channel);
}

// Internal method to write to the channel register
bool MPR121TouchSensor::updateGpio(byte reg, byte channel)
{
	// must be a GPIO channel
	ASSERT((channel >= 4 && channel <= 11) && (channel >= firstGpioChannel));
	// must be a Data Set, Data Clear or Data Toggle register
	ASSERT(reg == 0x78 || reg == 0x79 || reg == 0x7a);
	// register bit to set
	byte mask = MPR121_GPIO_MASK(channel);
	// channel must have been configured as an output
	ASSERT((gpioChannelConfigured & mask) && (gpioDirectionRegShadow & mask));

	return writeRegister(reg, mask);
}


// Write a single register
// most registers can only be written when in 'stop mode'
// ONLY the ECR and the GPIO registers can be written when in 'run mode'
// Call setStopMode() before writing, then setRunMode() when done.
// 'stop mode' is the default after calling begin() or softReset()
bool MPR121TouchSensor::writeRegister(byte reg, byte data)
{
	ASSERT(reg <= 0x80);

	// to write to most registers, the MPR121 must be in 'stop mode'
	// which means that electrode detection must be disabled via the
	// 'Electrode Configuration Register' ECR (0x5e)
	if (!inStopMode()) {
		// if not in 'stop mode', only the ECR register and the GPIO
		// registers can be written to
		if (reg != 0x5e && (reg < 0x73 || reg > 0x7a)) {
			LOGERROR("register cannot be written in run mode, see setStopMode()");
			return false;
		}
	}
	wire->beginTransmission(i2cAdds);
	wire->write(reg);
	wire->write(data);
	if (wire->endTransmission() != 0) {
		LOGERROR("endTransmission failed");
		return false;
	}

	#ifdef DEBUG
	// read back and verify, 
	// unless it's the soft reset or gpio register
	if (reg != 0x80 && (reg < 0x78 || reg > 0x7a)) {
		byte b;
		if (!readRegister(reg, &b))
			return false;
		if (b != data) {
			LOGERROR("verify failed");
			return false;
		}
	}
	#endif

	return true;
}

// Read one or more registers
bool MPR121TouchSensor::readRegister(byte reg, byte* data, byte nregs/*=1*/)
{
	ASSERT(reg + nregs <= 0x80);
	memset(data, 0, nregs);

	wire->beginTransmission(i2cAdds);
	wire->write(reg);
	if (wire->endTransmission(false) != 0) {
		LOGERROR("endTransmission failed")
			return false;
	}
	if (wire->requestFrom(i2cAdds, nregs) != nregs) {
		LOGERROR("requestFrom failed")
			return false;
	}
	if (wire->readBytes(data, nregs) != nregs) {
		LOGERROR("readBytes failed")
			return false;
	}
	return true;
}


#ifdef DEBUG
// Dump all the registers, p6
bool MPR121TouchSensor::dumpRegisters(Stream* stream)
{
	stream->println("\n\rMPR121 Registers");

	byte data[0x80];
	if (!readRegister(0, data, sizeof(data)))
		return false;
	for (uint i = 0; i < sizeof(data); ++i) {

		const char* s = NULL;
		switch (i) {
		case 0x00:
			s = "touch status";
			break;
		case 0x02:
			s = "out-of-range status OOR";
			break;
		case 0x04:
			s = "electrode filtered data";
			break;
		case 0x1e:
			s = "baseline values";
			break;
		case 0x2b:
			s = "baseline filtering control";
			break;
		case 0x36:
			stream->println();
			break;
		case 0x41:
			s = "touch release threshold";
			break;
		case 0x5b:
			s = "debounce";
			break;
		case 0x5c:
			s = "AFE and filter configuration CDC/CDT";
			break;
		case 0x5e:
			s = "electrode configuration ECR";
			break;
		case 0x5f:
			s = "electrode charge current";
			break;
		case 0x6c:
			s = "electrode charge time";
			break;
		case 0x73:
			s = "gpio";
			break;
		case 0x7b:
			s = "auto configuration";
			break;
		}
		if (s) {
			stream->println();
			stream->println(s);
		}
		stream->printf("0x%02X %02X\n\r", i, data[i]);
	}
	stream->println();
	stream->flush();
	return true;
}
#endif

