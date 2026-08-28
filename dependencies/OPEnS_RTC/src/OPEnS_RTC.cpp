//Code by JeeLabs http://news.jeelabs.org/code/
// Released to the public domain! Enjoy!

#include <Wire.h>
#include "OPEnS_RTC.h"
#ifdef __AVR__
 #include <avr/pgmspace.h>
 
#elif defined(ESP8266)
 #include <pgmspace.h>
#elif defined(ARDUINO_ARCH_SAMD)
// nothing special needed
#elif defined(ARDUINO_SAM_DUE)
 #define PROGMEM
 #define pgm_read_byte(addr) (*(const unsigned char *)(addr))
 #define Wire Wire
#endif

#define WIRE Wire

namespace {
constexpr uint8_t DS3231_I2C_OK = 0;
constexpr uint8_t DS3231_I2C_TX_BUFFER_ERROR = 1;
constexpr uint8_t DS3231_I2C_SHORT_READ = 5;
constexpr uint8_t DS3231_I2C_INVALID_DATA = 6;

uint8_t ds3231LastError = DS3231_I2C_OK;
bool ds3231LastOperationOk = true;
char legacyDateTimeText[24] = {};

uint8_t bcdToBinary(uint8_t value) { return value - 6 * (value >> 4); }

void setDS3231Result(bool ok, uint8_t error = DS3231_I2C_OK) {
	ds3231LastOperationOk = ok;
	ds3231LastError = ok ? DS3231_I2C_OK : error;
}

bool ds3231Write(uint8_t startRegister, const uint8_t *data, size_t length) {
	Wire.beginTransmission(DS3231_ADDRESS);
	if (Wire.write(startRegister) != 1) {
		Wire.endTransmission(true);
		setDS3231Result(false, DS3231_I2C_TX_BUFFER_ERROR);
		return false;
	}
	if (length > 0 && Wire.write(data, length) != length) {
		Wire.endTransmission(true);
		setDS3231Result(false, DS3231_I2C_TX_BUFFER_ERROR);
		return false;
	}
	const uint8_t error = Wire.endTransmission(true);
	setDS3231Result(error == 0, error);
	return error == 0;
}

bool ds3231WriteRegister(uint8_t reg, uint8_t value) {
	return ds3231Write(reg, &value, 1);
}

bool ds3231Read(uint8_t startRegister, uint8_t *data, size_t length) {
	if (data == nullptr || length == 0 || length > 255) {
		setDS3231Result(false, DS3231_I2C_INVALID_DATA);
		return false;
	}

	// Preserve the field-proven OPEnS transaction shape: finish the register
	// address write with STOP, then perform a separate read transaction.
	Wire.beginTransmission(DS3231_ADDRESS);
	if (Wire.write(startRegister) != 1) {
		Wire.endTransmission(true);
		setDS3231Result(false, DS3231_I2C_TX_BUFFER_ERROR);
		return false;
	}
	const uint8_t error = Wire.endTransmission(true);
	if (error != 0) {
		setDS3231Result(false, error);
		return false;
	}

	const size_t received = Wire.requestFrom(static_cast<uint8_t>(DS3231_ADDRESS),
	                                         static_cast<uint8_t>(length));
	if (received != length) {
		while (Wire.available())
			Wire.read();
		setDS3231Result(false, DS3231_I2C_SHORT_READ);
		return false;
	}
	for (size_t i = 0; i < length; ++i) {
		if (!Wire.available()) {
			setDS3231Result(false, DS3231_I2C_SHORT_READ);
			return false;
		}
		data[i] = static_cast<uint8_t>(Wire.read());
	}
	setDS3231Result(true);
	return true;
}

bool ds3231ReadRegister(uint8_t reg, uint8_t &value) {
	return ds3231Read(reg, &value, 1);
}

uint8_t decodeHour(uint8_t rawHour) {
	if ((rawHour & 0x40) == 0)
		return bcdToBinary(rawHour & 0x3F);

	uint8_t hour = bcdToBinary(rawHour & 0x1F);
	if (hour == 12)
		hour = 0;
	if (rawHour & 0x20)
		hour += 12;
	return hour;
}
} // namespace

#if (ARDUINO >= 100)
 #include <Arduino.h> // capital A so it is error prone on case-sensitive filesystems
 // Macro to deal with the difference in I2C write functions from old and new Arduino versions.
 #define _I2C_WRITE write
 #define _I2C_READ  read
#else
 #include <WProgram.h>
 #define _I2C_WRITE send
 #define _I2C_READ  receive
#endif

////////////////////////////////////////////////////////////////////////////////
// utility code, some of this could be exposed in the DateTime API if needed

const uint8_t daysInMonth [] PROGMEM = { 31,28,31,30,31,30,31,31,30,31,30,31 };

// number of days since 2000/01/01, valid for 2001..2099
static uint16_t date2days(uint16_t y, uint8_t m, uint8_t d) {
		if (y >= 2000)
				y -= 2000;
		uint16_t days = d;
		for (uint8_t i = 1; i < m; ++i)
				days += pgm_read_byte(daysInMonth + i - 1);
		if (m > 2 && y % 4 == 0)
				++days;
		return days + 365 * y + (y + 3) / 4 - 1;
}

static long time2long(uint16_t days, uint8_t h, uint8_t m, uint8_t s) {
		return ((days * 24L + h) * 60 + m) * 60 + s;
}


// Utilities for converting between BCD and binary
uint8_t bcd2bin (uint8_t val) { return val - 6 * (val >> 4); }
uint8_t bin2bcd (uint8_t val) { return val + 6 * (val / 10); }

// Effeciently convert two-digit byte to char array.
// Returns pointer to byte *AFTER* the most recent digit, to make repeated calls easy.
// Please make sure you pass in a valid pointer.
char * bin2char(char * src, uint8_t val){
	val = bin2bcd(val);
	*src++=(val>>4) + '0';
	*src++=(val & 0x0F) + '0';
	return src;
}


// Converts a 2-digit character array to an 8-bit unsigned int.
// Beware: There is no zero data validation here.
// If either digit is not a number, you will get an unexpected result.
// It will carry on happily as if nothing is wrong.
static uint8_t conv2d(const char* p) {
		uint8_t v = 0;
		if ('0' <= *p && *p <= '9')
				v = *p - '0';
		return 10 * v + *++p - '0';
}


////////////////////////////////////////////////////////////////////////////////
// DateTime implementation - ignores time zones and DST changes
// NOTE: also ignores leap seconds, see http://en.wikipedia.org/wiki/Leap_second

DateTime::DateTime (uint32_t t) {
	if (t < SECONDS_FROM_1970_TO_2000)
		t = SECONDS_FROM_1970_TO_2000;
	t -= SECONDS_FROM_1970_TO_2000;    // bring to 2000 timestamp from 1970

		ss = t % 60;
		t /= 60;
		mm = t % 60;
		t /= 60;
		hh = t % 24;
		uint16_t days = t / 24;
		uint8_t leap;
		for (yOff = 0; ; ++yOff) {
				leap = yOff % 4 == 0;
				if (days < 365 + leap)
						break;
				days -= 365 + leap;
		}
		for (m = 1; ; ++m) {
				uint8_t daysPerMonth = pgm_read_byte(daysInMonth + m - 1);
				if (leap && m == 2)
						++daysPerMonth;
				if (days < daysPerMonth)
						break;
				days -= daysPerMonth;
		}
		d = days + 1;
}

DateTime::DateTime (uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec) {
		if (year >= 2000)
				year -= 2000;
		yOff = year;
		m = month;
		d = day;
		hh = hour;
		mm = min;
		ss = sec;
}

DateTime::DateTime (const DateTime& copy):
	yOff(copy.yOff),
	m(copy.m),
	d(copy.d),
	hh(copy.hh),
	mm(copy.mm),
	ss(copy.ss)
{}

// A convenient constructor for using "the compiler's time":
//   DateTime now (__DATE__, __TIME__);
// NOTE: using F() would further reduce the RAM footprint, see below.
DateTime::DateTime (const char* date, const char* time) {
		// sample input: date = "Dec 26 2009", time = "12:34:56"
		yOff = conv2d(date + 9);
		// Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec 
		switch (date[0]) {
				case 'J': m = date[1] == 'a' ? 1 : (date[2] == 'n' ? 6 : 7); break;
				case 'F': m = 2; break;
				case 'A': m = date[2] == 'r' ? 4 : 8; break;
				case 'M': m = date[2] == 'r' ? 3 : 5; break;
				case 'S': m = 9; break;
				case 'O': m = 10; break;
				case 'N': m = 11; break;
				case 'D': m = 12; break;
		}
		d = conv2d(date + 4);
		hh = conv2d(time);
		mm = conv2d(time + 3);
		ss = conv2d(time + 6);
}

// A convenient constructor for using "the compiler's time":
// This version will save RAM by using PROGMEM to store it by using the F macro.
//   DateTime now (F(__DATE__), F(__TIME__));
DateTime::DateTime (const __FlashStringHelper* date, const __FlashStringHelper* time) {
		// sample input: date = "Dec 26 2009", time = "12:34:56"
		char buff[11];
		memcpy_P(buff, date, 11);
		yOff = conv2d(buff + 9);
		// Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec
		switch (buff[0]) {
				case 'J': m = buff[1] == 'a' ? 1 : (buff[2] == 'n' ? 6 : 7); break;
				case 'F': m = 2; break;
				case 'A': m = buff[2] == 'r' ? 4 : 8; break;
				case 'M': m = buff[2] == 'r' ? 3 : 5; break;
				case 'S': m = 9; break;
				case 'O': m = 10; break;
				case 'N': m = 11; break;
				case 'D': m = 12; break;
		}
		d = conv2d(buff + 4);
		memcpy_P(buff, time, 8);
		hh = conv2d(buff);
		mm = conv2d(buff + 3);
		ss = conv2d(buff + 6);
}

uint8_t DateTime::dayOfTheWeek() const {    
		uint16_t day = date2days(yOff, m, d);
		return (day + 6) % 7; // Jan 1, 2000 is a Saturday, i.e. returns 6
}

uint32_t DateTime::unixtime(void) const {
	uint32_t t;
	uint16_t days = date2days(yOff, m, d);
	t = time2long(days, hh, mm, ss);
	t += SECONDS_FROM_1970_TO_2000;  // seconds from 1970 to 2000

	return t;
}

long DateTime::secondstime(void) const {
	long t;
	uint16_t days = date2days(yOff, m, d);
	t = time2long(days, hh, mm, ss);
	return t;
}

char * DateTime::text(void) const {
	return text(legacyDateTimeText, sizeof(legacyDateTimeText));
}

char * DateTime::text(char *buffer, size_t length) const {
	if (buffer == nullptr || length == 0)
		return buffer;
	snprintf(buffer, length, "%04u.%02u.%02u %02u:%02u:%02u",
	         static_cast<unsigned int>(year()), static_cast<unsigned int>(month()),
	         static_cast<unsigned int>(day()), static_cast<unsigned int>(hour()),
	         static_cast<unsigned int>(minute()), static_cast<unsigned int>(second()));
	buffer[length - 1] = '\0';
	return buffer;
}

bool DateTime::isValid() const {
	if (m < 1 || m > 12 || hh > 23 || mm > 59 || ss > 59 || d < 1)
		return false;
	uint8_t maximumDay = pgm_read_byte(daysInMonth + m - 1);
	if (m == 2 && (yOff % 4) == 0)
		++maximumDay;
	return d <= maximumDay;
}

DateTime DateTime::operator+(const TimeSpan& span) const {
	return DateTime(unixtime()+span.totalseconds());
}

DateTime DateTime::operator-(const TimeSpan& span) const {
	return DateTime(unixtime()-span.totalseconds());
}

TimeSpan DateTime::operator-(const DateTime& right) const {
	return TimeSpan(unixtime()-right.unixtime());
}

////////////////////////////////////////////////////////////////////////////////
// TimeSpan implementation

TimeSpan::TimeSpan (int32_t seconds):
	_seconds(seconds)
{}

TimeSpan::TimeSpan (int16_t days, int8_t hours, int8_t minutes, int8_t seconds):
	_seconds((int32_t)days*86400L + (int32_t)hours*3600 + (int32_t)minutes*60 + seconds)
{}

TimeSpan::TimeSpan (const TimeSpan& copy):
	_seconds(copy._seconds)
{}

TimeSpan TimeSpan::operator+(const TimeSpan& right) const {
	return TimeSpan(_seconds+right._seconds);
}

TimeSpan TimeSpan::operator-(const TimeSpan& right) const {
	return TimeSpan(_seconds-right._seconds);
}

////////////////////////////////////////////////////////////////////////////////
// PCF8523 implementation

uint8_t PCF8523::begin(void) {
	// return 1;
	Wire.begin();
	return true;
}

// Example: bool a = PCF8523.isrunning();
// Returns 1 if RTC is running and 0 it's not 
uint8_t PCF8523::isrunning(void) {
	WIRE.beginTransmission(PCF8523_ADDRESS);
	WIRE._I2C_WRITE(0);
	WIRE.endTransmission();

	WIRE.requestFrom(PCF8523_ADDRESS, 1);
	uint8_t ss = WIRE._I2C_READ();
	ss = ss & 32;
	return !(ss>>5);
}


boolean PCF8523::initialized(void) {
	Wire.beginTransmission(PCF8523_ADDRESS);
	Wire._I2C_WRITE((byte)PCF8523_CONTROL_3);
	Wire.endTransmission();

	Wire.requestFrom(PCF8523_ADDRESS, 1);
	uint8_t ss = Wire._I2C_READ();
	return ((ss & 0xE0) != 0xE0);
}


// Example: PCF8523.adjust (DateTime(2014, 8, 14, 1, 49, 0))
// Sets RTC time to 2014/14/8 1:49 a.m.
void PCF8523::adjust(const DateTime& dt) {
	WIRE.beginTransmission(PCF8523_ADDRESS);
	WIRE._I2C_WRITE(0x03);
	WIRE._I2C_WRITE(bin2bcd(dt.second()));
	WIRE._I2C_WRITE(bin2bcd(dt.minute()));
	WIRE._I2C_WRITE(bin2bcd(dt.hour()));
	WIRE._I2C_WRITE(bin2bcd(dt.day()));
	WIRE._I2C_WRITE(bin2bcd(0));
	WIRE._I2C_WRITE(bin2bcd(dt.month()));
	WIRE._I2C_WRITE(bin2bcd(dt.year() - 2000));
	WIRE._I2C_WRITE(0);
	WIRE.endTransmission();
}

// Example: DateTime now = PCF8523.now();
// Returns Date and time in RTC in:
// year = now.year()
// month = now.month()
// day = now.day()
// hour = now.hour()
// minute = now.minute()
// second = now.second()
DateTime PCF8523::now() {
	WIRE.beginTransmission(PCF8523_ADDRESS);
	WIRE._I2C_WRITE(3);   
	WIRE.endTransmission();

	WIRE.requestFrom(PCF8523_ADDRESS, 7);
	uint8_t ss = bcd2bin(WIRE._I2C_READ() & 0x7F);
	uint8_t mm = bcd2bin(WIRE._I2C_READ());
	uint8_t hh = bcd2bin(WIRE._I2C_READ());
	uint8_t d = bcd2bin(WIRE._I2C_READ());
	WIRE._I2C_READ();
	uint8_t m = bcd2bin(WIRE._I2C_READ());
	uint16_t y = bcd2bin(WIRE._I2C_READ()) + 2000;
	
	return DateTime (y, m, d, hh, mm, ss);
}

// Example: PCF8523.read_reg(buf,size,address);
// Returns:   buf[0] = &address
//            buf[1] = &address + 1
//      ..... buf[size-1] = &address + size
void PCF8523::read_reg(uint8_t* buf, uint8_t size, uint8_t address) {
	int addrByte = address;
	WIRE.beginTransmission(PCF8523_ADDRESS);
	WIRE._I2C_WRITE(addrByte);
	WIRE.endTransmission();
	
	WIRE.requestFrom((uint8_t) PCF8523_ADDRESS, size);
	for (uint8_t pos = 0; pos < size; ++pos) {
		buf[pos] = WIRE._I2C_READ();
	}
}

// Example: PCF8523.write_reg(address,buf,size);
// Write:     buf[0] => &address
//            buf[1] => &address + 1
//      ..... buf[size-1] => &address + size
void PCF8523::write_reg(uint8_t address, uint8_t* buf, uint8_t size) {
	int addrByte = address;
	WIRE.beginTransmission(PCF8523_ADDRESS);
	WIRE._I2C_WRITE(addrByte);
	for (uint8_t pos = 0; pos < size; ++pos) {
		WIRE._I2C_WRITE(buf[pos]);
	}
	WIRE.endTransmission();
}

// Example: val = PCF8523.read_reg(0x08);
// Reads the value in register addressed at 0x08
// and returns data
uint8_t PCF8523::read_reg(uint8_t address) {
	uint8_t data;
	read_reg(&data, 1, address);
	return data;
}

// Example: PCF8523.write_reg(0x08, 0x25);
// Writes value 0x25 in register addressed at 0x08
void PCF8523::write_reg(uint8_t address, uint8_t data) {
	write_reg(address, &data, 1);
}

// Example: PCF8523.set_alarm(10,5,45)
// Set alarm at day = 5, 5:45 a.m.
void PCF8523::set_alarm(uint8_t day_alarm, uint8_t hour_alarm,uint8_t minute_alarm ) {
	WIRE.beginTransmission(PCF8523_ADDRESS);
	WIRE._I2C_WRITE(0x0A);
	// Enable Minute
	WIRE._I2C_WRITE(bin2bcd(minute_alarm) & ~0x80 );
	// Enable Hour
	WIRE._I2C_WRITE(bin2bcd(hour_alarm) & ~0x80 );
	// Enable Day
	WIRE._I2C_WRITE(bin2bcd(day_alarm) & ~0x80);
	WIRE._I2C_WRITE(0x80);	// Disable WeekDay
	WIRE.endTransmission();
}

void PCF8523::set_alarm(uint8_t hour_alarm,uint8_t minute_alarm ) {
	WIRE.beginTransmission(PCF8523_ADDRESS);
	WIRE._I2C_WRITE(0x0A);
	// Enable Minute
	WIRE._I2C_WRITE(bin2bcd(minute_alarm) & ~0x80 );
	// Enable Hour
	WIRE._I2C_WRITE(bin2bcd(hour_alarm) & ~0x80 );
	WIRE._I2C_WRITE(0x80);	// Disable Day	
	WIRE._I2C_WRITE(0x80);	// Disable WeekDay
	WIRE.endTransmission();
}

// = = = = = = = =
void PCF8523::set_alarm(uint8_t minute_alarm ) {
	WIRE.beginTransmission(PCF8523_ADDRESS);
	WIRE._I2C_WRITE(0x0A);
	// Enable Minute
	WIRE._I2C_WRITE(bin2bcd(minute_alarm) & ~0x80 );
	WIRE._I2C_WRITE(0x80);	// Disable Hour	
	WIRE._I2C_WRITE(0x80);	// Disable Day	
	WIRE._I2C_WRITE(0x80);	// Disable WeekDay
	WIRE.endTransmission();
}

void PCF8523::enable_alarm(bool enable)
{
	uint8_t tmp;

	tmp = read_reg(PCF8523_CONTROL_1);
	if(enable){
		// Disable Clockout & other Timers
		write_reg(PCF8523_TMR_CLKOUT_CTRL , 0x38);

		// Clear any existing flags
		ack_alarm();	
		// Enable the AIE bit
		tmp |= _BV(PCF8523_CONTROL_1_AIE_BIT);	

	}
	else {
		tmp &= ~_BV(PCF8523_CONTROL_1_AIE_BIT);	// Disable the AIE bit
	}
	write_reg(PCF8523_CONTROL_1 , tmp);

}

void PCF8523::ack_alarm(void)
{
	uint8_t tmp;
	tmp = read_reg(PCF8523_CONTROL_2);

	tmp &= ~_BV(PCF8523_CONTROL_2_AF_BIT);	// Clear the AF bit	

	write_reg(PCF8523_CONTROL_2 , tmp);
	return; 
}


Pcf8523SqwPinMode PCF8523::readSqwPinMode() {
	int mode;

	Wire.beginTransmission(PCF8523_ADDRESS);
	Wire._I2C_WRITE(PCF8523_CLKOUTCONTROL);
	Wire.endTransmission();
	
	Wire.requestFrom((uint8_t)PCF8523_ADDRESS, (uint8_t)1);
	mode = Wire._I2C_READ();

	mode >>= 3;
	mode &= 0x7;
	return static_cast<Pcf8523SqwPinMode>(mode);
}

void PCF8523::writeSqwPinMode(Pcf8523SqwPinMode mode) {
	Wire.beginTransmission(PCF8523_ADDRESS);
	Wire._I2C_WRITE(PCF8523_CLKOUTCONTROL);
	Wire._I2C_WRITE(mode << 3);
	Wire.endTransmission();
}

// = = = = = = = =

// Example: PCF8523.get_alarm(a);
// Returns a[0] = alarm minutes, a[1] = alarm hour, a[2] = alarm day
void PCF8523::get_alarm(uint8_t* buf) {
	WIRE.beginTransmission(PCF8523_ADDRESS);
	WIRE._I2C_WRITE(0x0A);
	WIRE.endTransmission();
	WIRE.requestFrom((uint8_t) PCF8523_ADDRESS, (uint8_t)3);
	for (uint8_t pos = 0; pos < 3; ++pos) {
		buf[pos] = bcd2bin((WIRE._I2C_READ() & 0x7F));
	}
	
}

void PCF8523::start_counter_1(uint8_t value){
		// Set timer freq at 1Hz
		write_reg(PCF8523_TMR_A_FREQ_CTRL , 2);
		// Load Timer value
		write_reg(PCF8523_TMR_A_REG,value); 
		// Set counter mode TAC[1:0] = 01 
		// Disable Clockout
		uint8_t tmp;
		tmp = read_reg(PCF8523_TMR_CLKOUT_CTRL);
		tmp |= (1<<7)|(1<<5)|(1<<4)|(1<<3)|(1<<1);
		tmp &= ~(1<<2);
		write_reg(PCF8523_TMR_CLKOUT_CTRL , tmp);
		// Set countdown flag CTAF
		// Enable interrupt CTAIE
		tmp = read_reg(PCF8523_CONTROL_2);
		tmp|=_BV(PCF8523_CONTROL_2_CTAF_BIT)|_BV(PCF8523_CONTROL_2_CTAIE_BIT);
		write_reg(PCF8523_CONTROL_2,tmp);
}

// Example: PCF8523.reset();
// Reset the PCF8523
void PCF8523::reset(){
	write_reg(PCF8523_CONTROL_1, 0x58);
}

uint8_t PCF8523::clear_rtc_interrupt_flags() {
	uint8_t rc2 = read_reg(PCF8523_CONTROL_2) & (PCF8523_CONTROL_2_SF_BIT | PCF8523_CONTROL_2_AF_BIT);
	write_reg(PCF8523_CONTROL_2, 0);  // Just zero the whole thing
	return rc2 != 0;
}

// Stop the default 32.768KHz CLKOUT signal on INT1.
void PCF8523::stop_32768_clkout() {
	uint8_t tmp = (read_reg (PCF8523_TMR_CLKOUT_CTRL))|RTC_CLKOUT_DISABLED;

	write_reg(PCF8523_TMR_CLKOUT_CTRL , tmp);
}

void PCF8523::setTimer1(eTIMER_TIMEBASE timebase, uint8_t value)
{
	uint8_t tmp;

	// Set the timebase
	write_reg(PCF8523_TMR_A_FREQ_CTRL , timebase);

	// Set the value
	write_reg(PCF8523_TMR_A_REG , value);

	// Clear any Timer A flags
    tmp = read_reg(PCF8523_CONTROL_2);
	
	tmp &= ~_BV(PCF8523_CONTROL_2_CTAF_BIT);	// Clear the Timer A flag
	tmp |= _BV(PCF8523_CONTROL_2_CTAIE_BIT);	// Enable Timer A interrupt

	write_reg(PCF8523_CONTROL_2 , tmp);

	// Set Timer A as Countdown and Enable
    tmp = read_reg(PCF8523_TMR_CLKOUT_CTRL);

	tmp |= _BV(PCF8523_TMR_CLKOUT_CTRL_TAM_BIT);	// /INT line is pulsed
	tmp |= 0x02;									// Set as a Countdown Timer	and Enable	

	write_reg(PCF8523_TMR_CLKOUT_CTRL , tmp);

}
void PCF8523::ackTimer1(void)
{
	uint8_t tmp;

	// Clear any Timer A flags
    tmp = read_reg(PCF8523_CONTROL_2);
	
	tmp &= ~_BV(PCF8523_CONTROL_2_CTAF_BIT);	// Clear the Timer A flag

	write_reg(PCF8523_CONTROL_2 , tmp);

	return;
}
uint8_t PCF8523::getTimer1(void)
{
	return read_reg(PCF8523_TMR_A_REG);
}
void PCF8523::setTimer2(eTIMER_TIMEBASE timebase,uint8_t value)
{
	uint8_t tmp;

	// Set the timebase
	write_reg(PCF8523_TMR_B_FREQ_CTRL , timebase);

	// Set the value
	write_reg(PCF8523_TMR_B_REG , value);

	// Clear any Timer B flags
  tmp = read_reg(PCF8523_CONTROL_2);
	
	tmp &= ~_BV(PCF8523_CONTROL_2_CTBF_BIT);	// Clear the Timer B flag
	tmp |= _BV(PCF8523_CONTROL_2_CTBIE_BIT);	// Enable Timer B interrupt

	write_reg(PCF8523_CONTROL_2 , tmp);

	// Set Timer A as Countdown and Enable
    tmp = read_reg(PCF8523_TMR_CLKOUT_CTRL);

	tmp |= _BV(PCF8523_TMR_CLKOUT_CTRL_TBM_BIT);	// /INT line is pulsed
	tmp |= 0x01;									// Enable	

	write_reg(PCF8523_TMR_CLKOUT_CTRL , tmp);


}
void PCF8523::ackTimer2(void)
{
	uint8_t tmp;

	// Clear any Timer B flags
  tmp = read_reg(PCF8523_CONTROL_2);
	
	tmp &= ~_BV(PCF8523_CONTROL_2_CTBF_BIT);	// Clear the Timer A flag

	write_reg(PCF8523_CONTROL_2 , tmp);

	return;
}
uint8_t PCF8523::getTimer2(void)
{
	return read_reg(PCF8523_TMR_B_REG);
}



////////////////////////////////////////////////////////////////////////////////
// RTC_DS1307 implementation



boolean RTC_DS1307::begin(void) {
	Wire.begin();
	return true;
}

uint8_t RTC_DS1307::isrunning(void) {
	Wire.beginTransmission(DS1307_ADDRESS);
	Wire._I2C_WRITE((byte)0);
	Wire.endTransmission();

	Wire.requestFrom(DS1307_ADDRESS, 1);
	uint8_t ss = Wire._I2C_READ();
	return !(ss>>7);
}

void RTC_DS1307::adjust(const DateTime& dt) {
	Wire.beginTransmission(DS1307_ADDRESS);
	Wire._I2C_WRITE((byte)0); // start at location 0
	Wire._I2C_WRITE(bin2bcd(dt.second()));
	Wire._I2C_WRITE(bin2bcd(dt.minute()));
	Wire._I2C_WRITE(bin2bcd(dt.hour()));
	Wire._I2C_WRITE(bin2bcd(0));
	Wire._I2C_WRITE(bin2bcd(dt.day()));
	Wire._I2C_WRITE(bin2bcd(dt.month()));
	Wire._I2C_WRITE(bin2bcd(dt.year() - 2000));
	Wire.endTransmission();
}

DateTime RTC_DS1307::now() {
	Wire.beginTransmission(DS1307_ADDRESS);
	Wire._I2C_WRITE((byte)0);	
	Wire.endTransmission();

	Wire.requestFrom(DS1307_ADDRESS, 7);
	uint8_t ss = bcd2bin(Wire._I2C_READ() & 0x7F);
	uint8_t mm = bcd2bin(Wire._I2C_READ());
	uint8_t hh = bcd2bin(Wire._I2C_READ());
	Wire._I2C_READ();
	uint8_t d = bcd2bin(Wire._I2C_READ());
	uint8_t m = bcd2bin(Wire._I2C_READ());
	uint16_t y = bcd2bin(Wire._I2C_READ()) + 2000;
	
	return DateTime (y, m, d, hh, mm, ss);
}

Ds1307SqwPinMode RTC_DS1307::readSqwPinMode() {
	int mode;

	Wire.beginTransmission(DS1307_ADDRESS);
	Wire._I2C_WRITE(DS1307_CONTROL);
	Wire.endTransmission();
	
	Wire.requestFrom((uint8_t)DS1307_ADDRESS, (uint8_t)1);
	mode = Wire._I2C_READ();

	mode &= 0x93;
	return static_cast<Ds1307SqwPinMode>(mode);
}

void RTC_DS1307::writeSqwPinMode(Ds1307SqwPinMode mode) {
	Wire.beginTransmission(DS1307_ADDRESS);
	Wire._I2C_WRITE(DS1307_CONTROL);
	Wire._I2C_WRITE(mode);
	Wire.endTransmission();
}

void RTC_DS1307::readnvram(uint8_t* buf, uint8_t size, uint8_t address) {
	int addrByte = DS1307_NVRAM + address;
	Wire.beginTransmission(DS1307_ADDRESS);
	Wire._I2C_WRITE(addrByte);
	Wire.endTransmission();
	
	Wire.requestFrom((uint8_t) DS1307_ADDRESS, size);
	for (uint8_t pos = 0; pos < size; ++pos) {
		buf[pos] = Wire._I2C_READ();
	}
}

void RTC_DS1307::writenvram(uint8_t address, uint8_t* buf, uint8_t size) {
	int addrByte = DS1307_NVRAM + address;
	Wire.beginTransmission(DS1307_ADDRESS);
	Wire._I2C_WRITE(addrByte);
	for (uint8_t pos = 0; pos < size; ++pos) {
		Wire._I2C_WRITE(buf[pos]);
	}
	Wire.endTransmission();
}

uint8_t RTC_DS1307::readnvram(uint8_t address) {
	uint8_t data;
	readnvram(&data, 1, address);
	return data;
}

void RTC_DS1307::writenvram(uint8_t address, uint8_t data) {
	writenvram(address, &data, 1);
}

////////////////////////////////////////////////////////////////////////////////
// RTC_Millis implementation

long RTC_Millis::offset = 0;

void RTC_Millis::adjust(const DateTime& dt) {
		offset = dt.unixtime() - millis() / 1000;
}

DateTime RTC_Millis::now() {
	return (uint32_t)(offset + millis() / 1000);
}

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// RTC_PCF8563 implementation

// boolean RTC_PCF8523::begin(void) {
// 	Wire.begin();
// 	return true;
// }

// boolean RTC_PCF8523::initialized(void) {
// 	Wire.beginTransmission(PCF8523_ADDRESS);
// 	Wire._I2C_WRITE((byte)PCF8523_CONTROL_3);
// 	Wire.endTransmission();

// 	Wire.requestFrom(PCF8523_ADDRESS, 1);
// 	uint8_t ss = Wire._I2C_READ();
// 	return ((ss & 0xE0) != 0xE0);
// }

// void RTC_PCF8523::adjust(const DateTime& dt) {
// 	Wire.beginTransmission(PCF8523_ADDRESS);
// 	Wire._I2C_WRITE((byte)3); // start at location 3
// 	Wire._I2C_WRITE(bin2bcd(dt.second()));
// 	Wire._I2C_WRITE(bin2bcd(dt.minute()));
// 	Wire._I2C_WRITE(bin2bcd(dt.hour()));
// 	Wire._I2C_WRITE(bin2bcd(dt.day()));
// 	Wire._I2C_WRITE(bin2bcd(0)); // skip weekdays
// 	Wire._I2C_WRITE(bin2bcd(dt.month()));
// 	Wire._I2C_WRITE(bin2bcd(dt.year() - 2000));
// 	Wire.endTransmission();

// 	// set to battery switchover mode
// 	Wire.beginTransmission(PCF8523_ADDRESS);
// 	Wire._I2C_WRITE((byte)PCF8523_CONTROL_3);
// 	Wire._I2C_WRITE((byte)0x00);
// 	Wire.endTransmission();
// }

// DateTime RTC_PCF8523::now() {
// 	Wire.beginTransmission(PCF8523_ADDRESS);
// 	Wire._I2C_WRITE((byte)3);	
// 	Wire.endTransmission();

// 	Wire.requestFrom(PCF8523_ADDRESS, 7);
// 	uint8_t ss = bcd2bin(Wire._I2C_READ() & 0x7F);
// 	uint8_t mm = bcd2bin(Wire._I2C_READ());
// 	uint8_t hh = bcd2bin(Wire._I2C_READ());
// 	uint8_t d = bcd2bin(Wire._I2C_READ());
// 	Wire._I2C_READ();  // skip 'weekdays'
// 	uint8_t m = bcd2bin(Wire._I2C_READ());
// 	uint16_t y = bcd2bin(Wire._I2C_READ()) + 2000;
	
// 	return DateTime (y, m, d, hh, mm, ss);
// }

// Pcf8523SqwPinMode RTC_PCF8523::readSqwPinMode() {
// 	int mode;

// 	Wire.beginTransmission(PCF8523_ADDRESS);
// 	Wire._I2C_WRITE(PCF8523_CLKOUTCONTROL);
// 	Wire.endTransmission();
	
// 	Wire.requestFrom((uint8_t)PCF8523_ADDRESS, (uint8_t)1);
// 	mode = Wire._I2C_READ();

// 	mode >>= 3;
// 	mode &= 0x7;
// 	return static_cast<Pcf8523SqwPinMode>(mode);
// }

// void RTC_PCF8523::writeSqwPinMode(Pcf8523SqwPinMode mode) {
// 	Wire.beginTransmission(PCF8523_ADDRESS);
// 	Wire._I2C_WRITE(PCF8523_CLKOUTCONTROL);
// 	Wire._I2C_WRITE(mode << 3);
// 	Wire.endTransmission();
// }

////////////////////////////////////////////////////////////////////////////////
// RTC_DS3231 implementation
//
// This implementation deliberately retains the transaction behavior that has
// been reliable in deployed Hypnos units:
//   * direct Wire access with a STOP between register selection and reads;
//   * alarm registers written before the alarm is armed;
//   * control and status written contiguously while arming;
//   * both alarm flags cleared before INT/SQW is relied on as a wake source.
//
// The hardening below adds validation, checked I2C results, readback, and no
// dynamic allocation. It does not introduce Adafruit BusIO or Arduino String.

boolean RTC_DS3231::begin(void) {
	Wire.begin();
	Wire.beginTransmission(DS3231_ADDRESS);
	const uint8_t error = Wire.endTransmission(true);
	if (error != 0) {
		setDS3231Result(false, error);
		return false;
	}

	uint8_t status = 0;
	return ds3231ReadRegister(DS3231_STATUSREG, status);
}

bool RTC_DS3231::lastOperationSucceeded() { return ds3231LastOperationOk; }

uint8_t RTC_DS3231::lastI2CError() { return ds3231LastError; }

bool RTC_DS3231::lostPower(void) {
	uint8_t status = 0;
	if (!ds3231ReadRegister(DS3231_STATUSREG, status))
		return true;
	return (status & 0x80) != 0;
}

bool RTC_DS3231::adjustChecked(const DateTime& dt) {
	if (!dt.isValid() || dt.year() < 2000 || dt.year() > 2099) {
		setDS3231Result(false, DS3231_I2C_INVALID_DATA);
		return false;
	}

	const uint8_t values[7] = {
		bin2bcd(dt.second()),
		bin2bcd(dt.minute()),
		bin2bcd(dt.hour()),
		bin2bcd(dt.dayOfTheWeek() + 1),
		bin2bcd(dt.day()),
		bin2bcd(dt.month()),
		bin2bcd(dt.year() - 2000),
	};
	if (!ds3231Write(0x00, values, sizeof(values)))
		return false;

	uint8_t status = 0;
	if (!ds3231ReadRegister(DS3231_STATUSREG, status))
		return false;
	status &= static_cast<uint8_t>(~0x80); // Clear OSF only after a verified time write.
	return ds3231WriteRegister(DS3231_STATUSREG, status);
}

void RTC_DS3231::adjust(const DateTime& dt) { adjustChecked(dt); }

DateTime RTC_DS3231::now() {
	uint8_t values[7] = {};
	if (!ds3231Read(0x00, values, sizeof(values)))
		return DateTime();

	const uint8_t second = bcd2bin(values[0] & 0x7F);
	const uint8_t minute = bcd2bin(values[1] & 0x7F);
	const uint8_t hour = decodeHour(values[2]);
	const uint8_t day = bcd2bin(values[4] & 0x3F);
	const uint8_t month = bcd2bin(values[5] & 0x1F);
	const uint16_t year = 2000U + bcd2bin(values[6]);
	const DateTime result(year, month, day, hour, minute, second);
	if (!result.isValid()) {
		setDS3231Result(false, DS3231_I2C_INVALID_DATA);
		return DateTime();
	}
	setDS3231Result(true);
	return result;
}

Ds3231SqwPinMode RTC_DS3231::readSqwPinMode() {
	uint8_t control = 0;
	if (!ds3231ReadRegister(DS3231_CONTROL, control))
		return DS3231_OFF;
	if (control & 0x04)
		return DS3231_OFF;
	return static_cast<Ds3231SqwPinMode>(control & 0x18);
}

bool RTC_DS3231::writeSqwPinMode(Ds3231SqwPinMode mode) {
	uint8_t control = 0;
	if (!ds3231ReadRegister(DS3231_CONTROL, control))
		return false;

	control &= static_cast<uint8_t>(~0x1C); // Clear INTCN and both rate bits.
	if (mode == DS3231_OFF)
		control |= 0x04; // Interrupt mode, square-wave output disabled.
	else
		control |= static_cast<uint8_t>(mode) & 0x18;
	return ds3231WriteRegister(DS3231_CONTROL, control);
}

float RTC_DS3231::getTemp() {
	uint8_t values[2] = {};
	if (!ds3231Read(DS3231_TEMP, values, sizeof(values)))
		return 0.0f;
	const int8_t wholeDegrees = static_cast<int8_t>(values[0]);
	return static_cast<float>(wholeDegrees) + static_cast<float>(values[1] >> 6) * 0.25f;
}

bool RTC_DS3231::setAlarm(Ds3231_ALARM_TYPES_t alarmType, byte seconds, byte minutes,
                          byte hours, byte daydate) {
	const uint8_t mode = static_cast<uint8_t>(alarmType) & 0x1F;
	const bool dayMatch = (mode & 0x10) != 0;
	const bool dayOrDateIsCompared = (mode & 0x08) == 0;
	const bool invalidDayOrDate =
		daydate > 31 || (dayOrDateIsCompared && daydate < 1) || (dayMatch && daydate > 7);
	if (seconds > 59 || minutes > 59 || hours > 23 || invalidDayOrDate) {
		setDS3231Result(false, DS3231_I2C_INVALID_DATA);
		return false;
	}

	const bool alarmTwo = (static_cast<uint8_t>(alarmType) & 0x80) != 0;
	uint8_t secondValue = bin2bcd(seconds);
	uint8_t minuteValue = bin2bcd(minutes);
	uint8_t hourValue = bin2bcd(hours);
	uint8_t dayValue = bin2bcd(daydate);

	if (mode & 0x01)
		secondValue |= _BV(A1M1);
	if (mode & 0x02)
		minuteValue |= _BV(A1M2);
	if (mode & 0x04)
		hourValue |= _BV(A1M3);
	if (mode & 0x08)
		dayValue |= _BV(A1M4);
	if (mode & 0x10)
		dayValue |= _BV(DYDT); // The legacy library accidentally changed the hour byte.

	const uint8_t values[4] = {secondValue, minuteValue, hourValue, dayValue};
	const bool written =
		alarmTwo ? ds3231Write(ALM2_MINUTES, values + 1, 3)
		         : ds3231Write(ALM1_SECONDS, values, 4);
	if (!written)
		return false;
	return armAlarm(alarmTwo ? 2 : 1, true);
}

bool RTC_DS3231::setAlarm(Ds3231_ALARM_TYPES_t alarmType, byte minutes, byte hours,
                          byte daydate) {
	return setAlarm(alarmType, 0, minutes, hours, daydate);
}

bool RTC_DS3231::setAlarm(const DateTime& t) {
	if (!t.isValid()) {
		setDS3231Result(false, DS3231_I2C_INVALID_DATA);
		return false;
	}
	return setAlarm(ALM1_MATCH_DATE, t.second(), t.minute(), t.hour(), t.day());
}

bool RTC_DS3231::setAlarm1(const DateTime& t, Ds3231Alarm1Mode alarmMode) {
	if (!t.isValid()) {
		setDS3231Result(false, DS3231_I2C_INVALID_DATA);
		return false;
	}
	const byte dayOrDate =
		alarmMode == DS3231_A1_Day ? static_cast<byte>(t.dayOfTheWeek() + 1) : t.day();
	return setAlarm(static_cast<Ds3231_ALARM_TYPES_t>(alarmMode), t.second(), t.minute(),
	                t.hour(), dayOrDate);
}

bool RTC_DS3231::setAlarm2(const DateTime& t, Ds3231Alarm2Mode alarmMode) {
	Ds3231_ALARM_TYPES_t legacyMode = ALM2_MATCH_DATE;
	switch (alarmMode) {
	case DS3231_A2_PerMinute:
		legacyMode = ALM2_EVERY_MINUTE;
		break;
	case DS3231_A2_Minute:
		legacyMode = ALM2_MATCH_MINUTES;
		break;
	case DS3231_A2_Hour:
		legacyMode = ALM2_MATCH_HOURS;
		break;
	case DS3231_A2_Day:
		legacyMode = ALM2_MATCH_DAY;
		break;
	case DS3231_A2_Date:
	default:
		legacyMode = ALM2_MATCH_DATE;
		break;
	}
	const byte dayOrDate =
		alarmMode == DS3231_A2_Day ? static_cast<byte>(t.dayOfTheWeek() + 1) : t.day();
	return setAlarm(legacyMode, 0, t.minute(), t.hour(), dayOrDate);
}

bool RTC_DS3231::armAlarm(byte alarmNumber, bool armed) {
	if (alarmNumber < 1 || alarmNumber > 2) {
		setDS3231Result(false, DS3231_I2C_INVALID_DATA);
		return false;
	}

	uint8_t control = 0;
	uint8_t status = 0;
	if (!ds3231ReadRegister(DS3231_CONTROL, control) ||
	    !ds3231ReadRegister(DS3231_STATUSREG, status))
		return false;

	const uint8_t enableMask = _BV(alarmNumber - 1);
	if (armed)
		control |= enableMask;
	else
		control &= static_cast<uint8_t>(~enableMask);

	// Retain the proven OPEnS control-state normalization: oscillator enabled,
	// battery-backed square wave off, conversion request clear, and INTCN set.
	control = static_cast<uint8_t>((control & 0x1F) | 0x04);

	// Preserve OSF and EN32kHz while clearing both alarm flags. The legacy code
	// wrote zero here; only A1F/A2F need to change to release INT/SQW.
	status &= static_cast<uint8_t>(~0x03);
	const uint8_t controlAndStatus[2] = {control, status};
	if (!ds3231Write(DS3231_CONTROL, controlAndStatus, sizeof(controlAndStatus)))
		return false;

	uint8_t readback[2] = {};
	if (!ds3231Read(DS3231_CONTROL, readback, sizeof(readback)))
		return false;
	const bool enableMatches = ((readback[0] & enableMask) != 0) == armed;
	const bool interruptMode = (readback[0] & 0x04) != 0;
	const bool flagsClear = (readback[1] & 0x03) == 0;
	const bool verified = enableMatches && interruptMode && flagsClear;
	setDS3231Result(verified, verified ? DS3231_I2C_OK : DS3231_I2C_INVALID_DATA);
	return verified;
}

bool RTC_DS3231::clearAlarm(byte alarmNumber) {
	if (alarmNumber < 1 || alarmNumber > 3) {
		setDS3231Result(false, DS3231_I2C_INVALID_DATA);
		return false;
	}
	uint8_t status = 0;
	if (!ds3231ReadRegister(DS3231_STATUSREG, status))
		return false;
	status &= static_cast<uint8_t>(~(alarmNumber & 0x03));
	return ds3231WriteRegister(DS3231_STATUSREG, status);
}

bool RTC_DS3231::clearAlarm(void) { return clearAlarm(3); }

bool RTC_DS3231::alarmFired(byte alarmNumber) {
	if (alarmNumber < 1 || alarmNumber > 2) {
		setDS3231Result(false, DS3231_I2C_INVALID_DATA);
		return false;
	}
	uint8_t status = 0;
	if (!ds3231ReadRegister(DS3231_STATUSREG, status))
		return false;
	return (status & _BV(alarmNumber - 1)) != 0;
}

bool RTC_DS3231::isArmed(byte alarmNumber) {
	if (alarmNumber < 1 || alarmNumber > 2) {
		setDS3231Result(false, DS3231_I2C_INVALID_DATA);
		return false;
	}
	uint8_t control = 0;
	if (!ds3231ReadRegister(DS3231_CONTROL, control))
		return false;
	return (control & _BV(alarmNumber - 1)) != 0;
}

DateTime RTC_DS3231::getAlarm(byte alarmNumber) {
	if (alarmNumber < 1 || alarmNumber > 2) {
		setDS3231Result(false, DS3231_I2C_INVALID_DATA);
		return DateTime();
	}

	uint8_t values[4] = {};
	const size_t length = alarmNumber == 1 ? 4 : 3;
	const uint8_t startRegister = alarmNumber == 1 ? ALM1_SECONDS : ALM2_MINUTES;
	if (!ds3231Read(startRegister, values, length))
		return DateTime();

	const uint8_t offset = alarmNumber == 1 ? 0 : 1;
	const uint8_t second = alarmNumber == 1 ? bcd2bin(values[0] & 0x7F) : 0;
	const uint8_t minute = bcd2bin(values[1 - offset] & 0x7F);
	const uint8_t hour = decodeHour(values[2 - offset]);
	const uint8_t day = bcd2bin(values[3 - offset] & 0x3F);

	const DateTime current = now();
	if (!lastOperationSucceeded())
		return DateTime();
	const DateTime result(current.year(), current.month(), day, hour, minute, second);
	if (!result.isValid()) {
		setDS3231Result(false, DS3231_I2C_INVALID_DATA);
		return DateTime();
	}
	setDS3231Result(true);
	return result;
}

Ds3231Alarm1Mode RTC_DS3231::getAlarm1Mode() {
	uint8_t values[4] = {};
	if (!ds3231Read(ALM1_SECONDS, values, sizeof(values)))
		return DS3231_A1_Date;
	const uint8_t mode = ((values[0] & 0x80) >> 7) | ((values[1] & 0x80) >> 6) |
	                     ((values[2] & 0x80) >> 5) | ((values[3] & 0x80) >> 4) |
	                     ((values[3] & 0x40) >> 2);
	switch (mode) {
	case DS3231_A1_PerSecond:
	case DS3231_A1_Second:
	case DS3231_A1_Minute:
	case DS3231_A1_Hour:
	case DS3231_A1_Day:
	case DS3231_A1_Date:
		return static_cast<Ds3231Alarm1Mode>(mode);
	default:
		setDS3231Result(false, DS3231_I2C_INVALID_DATA);
		return DS3231_A1_Date;
	}
}

Ds3231Alarm2Mode RTC_DS3231::getAlarm2Mode() {
	uint8_t values[3] = {};
	if (!ds3231Read(ALM2_MINUTES, values, sizeof(values)))
		return DS3231_A2_Date;
	const uint8_t mode = ((values[0] & 0x80) >> 7) | ((values[1] & 0x80) >> 6) |
	                     ((values[2] & 0x80) >> 5) | ((values[2] & 0x40) >> 3);
	switch (mode) {
	case DS3231_A2_PerMinute:
	case DS3231_A2_Minute:
	case DS3231_A2_Hour:
	case DS3231_A2_Day:
	case DS3231_A2_Date:
		return static_cast<Ds3231Alarm2Mode>(mode);
	default:
		setDS3231Result(false, DS3231_I2C_INVALID_DATA);
		return DS3231_A2_Date;
	}
}

bool RTC_DS3231::forceConversion(void) {
	uint8_t control = 0;
	if (!ds3231ReadRegister(DS3231_CONTROL, control))
		return false;
	control |= 0x20;
	if (!ds3231WriteRegister(DS3231_CONTROL, control))
		return false;

	const uint32_t started = millis();
	while (static_cast<uint32_t>(millis() - started) < 1000UL) {
		uint8_t status = 0;
		if (!ds3231ReadRegister(DS3231_STATUSREG, status))
			return false;
		if ((status & 0x04) == 0)
			return true;
		delay(2);
	}
	setDS3231Result(false, DS3231_I2C_INVALID_DATA);
	return false;
}
