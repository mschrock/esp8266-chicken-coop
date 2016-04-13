// the code in this file is copied from https://github.com/trunet/DS3231RTC
// the reason is that if I keep it in the library as a class, is in conflict with some other library and crashes my esp8266
// If I put it in a separate file, like bellow, it works well.

#if defined (__RTC_INSTALLED__)

// registers
#define DS3231_CONTROL_ADDR         0x0E
#define DS3231_STATUS_ADDR          0x0F

#define DS3231_CTRL_ID 104


// PUBLIC FUNCTIONS
time_t rtc_get()   // Aquire data from buffer and convert to time_t
{
	tmElements_t tm;
	rtc_read(tm);
	return(makeTime(tm));
}

void  rtc_set(time_t t)
{
	tmElements_t tm;
	breakTime(t, tm);
	tm.Second |= 0x80;  // stop the clock
	rtc_write(tm);
	tm.Second &= 0x7f;  // start the clock
	rtc_write(tm);
}

// Aquire data from the RTC chip in BCD format
void rtc_read(tmElements_t &tm)
{
	Wire.beginTransmission(DS3231_CTRL_ID);
	Wire.write((uint8_t)0);
	Wire.endTransmission();

	// request the 7 data fields   (secs, min, hr, dow, date, mth, yr)
	Wire.requestFrom(DS3231_CTRL_ID, tmNbrFields);

	tm.Second = rtc_bcd2dec(Wire.read() & 0x7f);
	tm.Minute = rtc_bcd2dec(Wire.read());
	tm.Hour = rtc_bcd2dec(Wire.read() & 0x3f);  // mask assumes 24hr clock
	tm.Wday = rtc_bcd2dec(Wire.read());
	tm.Day = rtc_bcd2dec(Wire.read());
	tm.Month = rtc_bcd2dec(Wire.read());
	tm.Year = y2kYearToTm((rtc_bcd2dec(Wire.read())));
}

void rtc_write(tmElements_t &tm)
{
	Wire.beginTransmission(DS3231_CTRL_ID);
	Wire.write((uint8_t)0); // reset register pointer

	Wire.write(rtc_dec2bcd(tm.Second));
	Wire.write(rtc_dec2bcd(tm.Minute));
	Wire.write(rtc_dec2bcd(tm.Hour));      // sets 24 hour format
	Wire.write(rtc_dec2bcd(tm.Wday));
	Wire.write(rtc_dec2bcd(tm.Day));
	Wire.write(rtc_dec2bcd(tm.Month));
	Wire.write(rtc_dec2bcd(tmYearToY2k(tm.Year)));

	Wire.endTransmission();
}

float rtc_getTemp()
{
	byte tMSB, tLSB;
	float temp3231;

	//temp registers (11h-12h) get updated automatically every 64s
	Wire.beginTransmission(DS3231_CTRL_ID);
	Wire.write(0x11);
	Wire.endTransmission();
	Wire.requestFrom(DS3231_CTRL_ID, 2);

	if (Wire.available()) {
		tMSB = Wire.read(); //2's complement int portion
		tLSB = Wire.read(); //fraction portion

		temp3231 = (tMSB & B01111111); //do 2's math on Tmsb
		temp3231 += ((tLSB >> 6) * 0.25); //only care about bits 7 & 8
										  //temp3231 = ((((short)tMSB << 8) | (short)tLSB) >> 6) / 4.0;
										  //temp3231 = (temp3231 * 1.8) + 32.0;
		return temp3231;
	}
	else {
		//oh noes, no data!
	}

	return 0;
}
// PRIVATE FUNCTIONS

// Convert Decimal to Binary Coded Decimal (BCD)
uint8_t rtc_dec2bcd(uint8_t num)
{
	return ((num / 10 * 16) + (num % 10));
}

// Convert Binary Coded Decimal (BCD) to Decimal
uint8_t rtc_bcd2dec(uint8_t num)
{
	return ((num / 16 * 10) + (num % 16));
}

// PROTECTED FUNCTIONS

void rtc_set_sreg(uint8_t val) {
	Wire.beginTransmission(DS3231_CTRL_ID);
	Wire.write(DS3231_STATUS_ADDR);
	Wire.write(val);
	Wire.endTransmission();
}

uint8_t rtc_get_sreg() {
	uint8_t rv;

	Wire.beginTransmission(DS3231_CTRL_ID);
	Wire.write(DS3231_STATUS_ADDR);
	Wire.endTransmission();

	Wire.requestFrom(DS3231_CTRL_ID, 1);
	rv = Wire.read();

	return rv;
}

void rtc_set_creg(uint8_t val) {
	Wire.beginTransmission(DS3231_CTRL_ID);
	Wire.write(DS3231_CONTROL_ADDR);
	Wire.write(val);
	Wire.endTransmission();
}

uint8_t rtc_get_creg() {
	uint8_t rv;

	Wire.beginTransmission(DS3231_CTRL_ID);
	Wire.write(DS3231_CONTROL_ADDR);
	Wire.endTransmission();

	Wire.requestFrom(DS3231_CTRL_ID, 1);
	rv = Wire.read();

	return rv;
}

#endif