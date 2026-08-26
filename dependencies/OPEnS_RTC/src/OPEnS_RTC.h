// Code by JeeLabs http://news.jeelabs.org/code/
// Released to the public domain! Enjoy!

#ifndef OPENS_RTC_H
#define OPENS_RTC_H

// Identifies the checked, allocation-free Loom fork. Release packaging must select this copy
// rather than an older user-installed OPEnS_RTC library with the same header name.
#define LOOM_OPENS_RTC_PATCH_LEVEL 1

#include <Arduino.h>
class TimeSpan;

// NOTE: Must include the following line to use 
// with M0 (was not in original RTClibExtended.h):
#ifndef _BV
#define _BV(bit) (1U << (bit))
#endif


//Begin PCF8523 definitions
#define PCF8523_ADDRESS				0x68
#define PCF8523_CLKOUTCONTROL		0x0F
#define PCF8523_CONTROL_1			0x00
#define PCF8523_CONTROL_2			0x01
#define PCF8523_CONTROL_3			0x02

#define PCF8523_SECONDS				0x03
#define PCF8523_MINUTES				0x04
#define PCF8523_HOURS				0x05
#define PCF8523_DAYS				0x06
#define PCF8523_WEEKDAYS			0x07
#define PCF8523_MONTHS				0x08
#define PCF8523_YEARS				0x09
#define PCF8523_MINUTE_ALARM		0x0A
#define PCF8523_HOUR_ALARM			0x0B
#define PCF8523_DAY_ALARM			0x0C
#define PCF8523_WEEKDAY_ALARM		0x0D
#define PCF8523_OFFSET				0x0E
#define PCF8523_TMR_CLKOUT_CTRL		0x0F
#define PCF8523_TMR_A_FREQ_CTRL		0x10
#define PCF8523_TMR_A_REG			0x11
#define PCF8523_TMR_B_FREQ_CTRL		0x12
#define PCF8523_TMR_B_REG			0x13

#define PCF8523_CONTROL_1_CAP_SEL_BIT	7
#define PCF8523_CONTROL_1_T_BIT			6
#define PCF8523_CONTROL_1_STOP_BIT		5
#define PCF8523_CONTROL_1_SR_BIT		4
#define PCF8523_CONTROL_1_1224_BIT		3
#define PCF8523_CONTROL_1_SIE_BIT		2
#define PCF8523_CONTROL_1_AIE_BIT		1
#define PCF8523_CONTROL_1CIE_BIT		0

#define PCF8523_CONTROL_2_WTAF_BIT		7
#define PCF8523_CONTROL_2_CTAF_BIT		6
#define PCF8523_CONTROL_2_CTBF_BIT		5
#define PCF8523_CONTROL_2_SF_BIT		4
#define PCF8523_CONTROL_2_AF_BIT 		3
#define PCF8523_CONTROL_2_WTAIE_BIT		2
#define PCF8523_CONTROL_2_CTAIE_BIT		1
#define PCF8523_CONTROL_2_CTBIE_BIT		0

#define PCF8523_SECONDS_OS_BIT			7
#define PCF8523_SECONDS_10_BIT       	6
#define PCF8523_SECONDS_10_LENGTH   	3
#define PCF8523_SECONDS_1_BIT        	3
#define PCF8523_SECONDS_1_LENGTH     	4

#define PCF8523_MINUTES_10_BIT       	6
#define PCF8523_MINUTES_10_LENGTH    	3
#define PCF8523_MINUTES_1_BIT        	3
#define PCF8523_MINUTES_1_LENGTH     	4

#define PCF8523_HOURS_MODE_BIT  	    3 // 0 = 24-hour mode, 1 = 12-hour mode
#define PCF8523_HOURS_AMPM_BIT      	5 // 2nd HOURS_10 bit if in 24-hour mode
#define PCF8523_HOURS_10_BIT        	4
#define PCF8523_HOURS_1_BIT          	3
#define PCF8523_HOURS_1_LENGTH       	4

#define PCF8523_WEEKDAYS_BIT 	        2
#define PCF8523_WEEKDAYS_LENGTH         3

#define PCF8523_DAYS_10_BIT          5
#define PCF8523_DAYS_10_LENGTH       2
#define PCF8523_DAYS_1_BIT           3
#define PCF8523_DAYS_1_LENGTH        4

#define PCF8523_MONTH_10_BIT         4
#define PCF8523_MONTH_1_BIT          3
#define PCF8523_MONTH_1_LENGTH       4

#define PCF8523_YEAR_10H_BIT         7
#define PCF8523_YEAR_10H_LENGTH      4
#define PCF8523_YEAR_1H_BIT          3
#define PCF8523_YEAR_1H_LENGTH       4


#define PCF8523_TMR_CLKOUT_CTRL_TAM_BIT		7
#define PCF8523_TMR_CLKOUT_CTRL_TBM_BIT		6
#define PCF8523_TMR_CLKOUT_CTRL_TBC_BIT		0

//End PCF8523 defines

#define DS1307_ADDRESS               	0x68
#define DS1307_CONTROL               	0x07
#define DS1307_NVRAM                 	0x08

#define DS3231_ADDRESS               	0x68
#define DS3231_CONTROL               	0x0E
#define DS3231_STATUSREG             	0x0F
#define DS3231_TEMP                  	0x11

#define SECONDS_PER_DAY              	86400L

#define SECONDS_FROM_1970_TO_2000    	946684800UL

//Control register bits
#define A1IE 0
#define A2IE 1

//Alarm mask bits
#define A1M1 7
#define A1M2 7
#define A1M3 7
#define A1M4 7
#define A2M2 7
#define A2M3 7
#define A2M4 7

//DS3231 Register Addresses
#define ALM1_SECONDS 0x07
#define ALM1_MINUTES 0x08
#define ALM1_HOURS 0x09
#define ALM1_DAYDATE 0x0A
#define ALM2_MINUTES 0x0B
#define ALM2_HOURS 0x0C
#define ALM2_DAYDATE 0x0D

//Other
#define DYDT 6                     //Day/Date flag bit in alarm Day/Date registers

const uint8_t RTC_CLKOUT_DISABLED = ((1<<3) | (1<<4) | (1<<5));

// Simple general-purpose date/time class (no TZ / DST / leap second handling!)
class DateTime {
public:
	DateTime (uint32_t t = SECONDS_FROM_1970_TO_2000);
	DateTime (uint16_t year, uint8_t month, uint8_t day,
				uint8_t hour =0, uint8_t min =0, uint8_t sec =0);
	DateTime (const DateTime& copy);
	DateTime (const char* date, const char* time);
	DateTime (const __FlashStringHelper* date, const __FlashStringHelper* time);
	uint16_t year() const       { return 2000 + yOff; }
	uint8_t month() const       { return m; }
	uint8_t day() const         { return d; }
	uint8_t hour() const        { return hh; }
	uint8_t minute() const      { return mm; }
	uint8_t second() const      { return ss; }
	uint8_t dayOfTheWeek() const;

	// 32-bit times as seconds since 1/1/2000
	long secondstime() const;   
	// 32-bit times as seconds since 1/1/1970
	uint32_t unixtime(void) const;
	// String representation of time
	// Legacy formatting API. The returned buffer is shared and is overwritten by
	// the next text() call; prefer the caller-owned overload below.
	char * text() const;
	char * text(char *buffer, size_t length) const;
	bool isValid() const;

	DateTime operator+(const TimeSpan& span) const;
	DateTime operator-(const TimeSpan& span) const;
	TimeSpan operator-(const DateTime& right) const;

protected:
	uint8_t yOff, m, d, hh, mm, ss;
};

// Timespan which can represent changes in time with seconds accuracy.
class TimeSpan {
public:
	TimeSpan (int32_t seconds = 0);
	TimeSpan (int16_t days, int8_t hours, int8_t minutes, int8_t seconds);
	TimeSpan (const TimeSpan& copy);
	int16_t days() const         { return _seconds / 86400L; }
	int8_t  hours() const        { return _seconds / 3600 % 24; }
	int8_t  minutes() const      { return _seconds / 60 % 60; }
	int8_t  seconds() const      { return _seconds % 60; }
	int32_t totalseconds() const { return _seconds; }

	TimeSpan operator+(const TimeSpan& right) const;
	TimeSpan operator-(const TimeSpan& right) const;

protected:
	int32_t _seconds;
};


enum Pcf8523SqwPinMode { PCF8523_OFF = 7, PCF8523_SquareWave1HZ = 6, PCF8523_SquareWave32HZ = 5, PCF8523_SquareWave1kHz = 4, PCF8523_SquareWave4kHz = 3, PCF8523_SquareWave8kHz = 2, PCF8523_SquareWave16kHz = 1, PCF8523_SquareWave32kHz = 0 };

typedef enum {
	eTB_4KHZ = 0,
	eTB_64HZ,
	eTB_SECOND,
	eTB_MINUTE,
	eTB_HOUR
} eTIMER_TIMEBASE;

class PCF8523{

	public:
	static uint8_t begin(void);
	static void adjust(const DateTime& dt);
	boolean initialized(void);
	uint8_t isrunning(void);
	static DateTime now();
	uint8_t read_reg(uint8_t address);
	void read_reg(uint8_t* buf, uint8_t size, uint8_t address);
	void write_reg(uint8_t address, uint8_t data);
	void write_reg(uint8_t address, uint8_t* buf, uint8_t size);
	void set_alarm(uint8_t day_alarm, uint8_t hour_alarm,uint8_t minute_alarm ) ;
	void set_alarm(uint8_t hour_alarm,uint8_t minute_alarm );
	void set_alarm(uint8_t minute_alarm );
	void get_alarm(uint8_t* buf);
	void reset();
	uint8_t clear_rtc_interrupt_flags(); 
	void stop_32768_clkout();
	void start_counter_1(uint8_t value);

	void enable_alarm(bool enable);
	void ack_alarm(void);
	Pcf8523SqwPinMode readSqwPinMode();
	void writeSqwPinMode(Pcf8523SqwPinMode mode);

	// Periodic Timers
	void setTimer1(eTIMER_TIMEBASE timebase, uint8_t value);
	void ackTimer1(void);
	uint8_t getTimer1(void);
	void setTimer2(eTIMER_TIMEBASE timebase,uint8_t value);
	void ackTimer2(void);
	uint8_t getTimer2(void);

};

enum Ds1307SqwPinMode { OFF = 0x00, ON = 0x80, SquareWave1HZ = 0x10, SquareWave4kHz = 0x11, SquareWave8kHz = 0x12, SquareWave32kHz = 0x13 };

class RTC_DS1307 {
public:
	boolean begin(void);
	static void adjust(const DateTime& dt);
	uint8_t isrunning(void);
	static DateTime now();
	static Ds1307SqwPinMode readSqwPinMode();
	static void writeSqwPinMode(Ds1307SqwPinMode mode);
	uint8_t readnvram(uint8_t address);
	void readnvram(uint8_t* buf, uint8_t size, uint8_t address);
	void writenvram(uint8_t address, uint8_t data);
	void writenvram(uint8_t address, uint8_t* buf, uint8_t size);
};

// RTC based on the DS3231 chip connected via I2C and the Wire library
enum Ds3231SqwPinMode { DS3231_OFF = 0x01, DS3231_SquareWave1Hz = 0x00, DS3231_SquareWave1kHz = 0x08, DS3231_SquareWave4kHz = 0x10, DS3231_SquareWave8kHz = 0x18 };

//Alarm masks
enum Ds3231_ALARM_TYPES_t {
	ALM1_EVERY_SECOND = 0x0F,
	ALM1_MATCH_SECONDS = 0x0E,
	ALM1_MATCH_MINUTES = 0x0C,     //match minutes *and* seconds
	ALM1_MATCH_HOURS = 0x08,       //match hours *and* minutes, seconds
	ALM1_MATCH_DATE = 0x00,        //match date *and* hours, minutes, seconds
	ALM1_MATCH_DAY = 0x10,         //match day *and* hours, minutes, seconds

	ALM2_EVERY_MINUTE = 0x8E,
	ALM2_MATCH_MINUTES = 0x8C,     //match minutes
	ALM2_MATCH_HOURS = 0x88,       //match hours *and* minutes
	ALM2_MATCH_DATE = 0x80,        //match date *and* hours, minutes
	ALM2_MATCH_DAY = 0x90,         //match day *and* hours, minutes
};

// Adafruit RTClib-compatible alarm names. Values map directly to the DS3231
// mask bits and allow Loom to retain its guarded alarm/readback path.
enum Ds3231Alarm1Mode {
	DS3231_A1_PerSecond = 0x0F,
	DS3231_A1_Second = 0x0E,
	DS3231_A1_Minute = 0x0C,
	DS3231_A1_Hour = 0x08,
	DS3231_A1_Date = 0x00,
	DS3231_A1_Day = 0x10,
};

enum Ds3231Alarm2Mode {
	DS3231_A2_PerMinute = 0x07,
	DS3231_A2_Minute = 0x06,
	DS3231_A2_Hour = 0x04,
	DS3231_A2_Date = 0x00,
	DS3231_A2_Day = 0x08,
};

class RTC_DS3231 {
public:
	boolean begin(void);
	static void adjust(const DateTime& dt);
	static bool adjustChecked(const DateTime& dt);
	bool lostPower(void);
	static DateTime now();
	static Ds3231SqwPinMode readSqwPinMode();
	static bool writeSqwPinMode(Ds3231SqwPinMode mode);
	float getTemp();
	float getTemperature() { return getTemp(); }
	bool forceConversion(void);
	bool setAlarm(Ds3231_ALARM_TYPES_t alarmType, byte seconds, byte minutes, byte hours, byte daydate);
	bool setAlarm(Ds3231_ALARM_TYPES_t alarmType, byte minutes, byte hours, byte daydate);
	bool setAlarm(const DateTime& t);
	bool setAlarm1(const DateTime& t, Ds3231Alarm1Mode alarmMode);
	bool setAlarm2(const DateTime& t, Ds3231Alarm2Mode alarmMode);
	bool armAlarm(byte alarmNumber, bool armed);
	bool disableAlarm(byte alarmNumber) { return armAlarm(alarmNumber, false); }
	bool isArmed(byte alarmNumber);
	bool clearAlarm(byte alarmNumber);
	bool clearAlarm(void);
	bool alarmFired(byte alarmNumber);
	DateTime getAlarm(byte alarmNumber);
	DateTime getAlarm1() { return getAlarm(1); }
	DateTime getAlarm2() { return getAlarm(2); }
	Ds3231Alarm1Mode getAlarm1Mode();
	Ds3231Alarm2Mode getAlarm2Mode();
	static bool lastOperationSucceeded();
	static uint8_t lastI2CError();
};

// RTC based on the PCF8523 chip connected via I2C and the Wire library
// enum Pcf8523SqwPinMode { PCF8523_OFF = 7, PCF8523_SquareWave1HZ = 6, PCF8523_SquareWave32HZ = 5, PCF8523_SquareWave1kHz = 4, PCF8523_SquareWave4kHz = 3, PCF8523_SquareWave8kHz = 2, PCF8523_SquareWave16kHz = 1, PCF8523_SquareWave32kHz = 0 };

// class RTC_PCF8523 {
// public:
// 	boolean begin(void);
// 	void adjust(const DateTime& dt);
// 	boolean initialized(void);
// 	static DateTime now();

// 	Pcf8523SqwPinMode readSqwPinMode();
// 	void writeSqwPinMode(Pcf8523SqwPinMode mode);
// };

// RTC using the internal millis() clock, has to be initialized before use
// NOTE: this clock won't be correct once the millis() timer rolls over (>49d?)
class RTC_Millis {
public:
	static void begin(const DateTime& dt) { adjust(dt); }
	static void adjust(const DateTime& dt);
	static DateTime now();

protected:
	static long offset;
};

#endif // OPENS_RTC_H
