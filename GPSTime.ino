#if defined (__GPS_INSTALLED__)

#include <TinyGPS++.h>            //https://github.com/mikalhart/TinyGPSPlus
#include <SoftwareSerial.h>

// we will syncronize the time with an GPS connected tot pin gpo13 -> gps TX, gpo15 -> gpx RX
static const int RXPin = 13; // gpo13 - d7
static const int TXPin = 15; // gpo15 - d8
							 // change the speed to match your GPS 
static const uint32_t GPSBaud = 38400;

SoftwareSerial SerialGPS = SoftwareSerial(RXPin, TXPin);
TinyGPSPlus gps;

void initGPS()
{
	SerialGPS.begin(GPSBaud);
}

boolean getGPSUnixTime()
{
	DEBUG_println("Please wait, syncing the clock with GPS...");
	// Dispatch incoming characters
	while (SerialGPS.available() > 0)
		gps.encode(SerialGPS.read());

	if (gps.date.isUpdated() && gps.time.isUpdated() && gps.location.isUpdated() && gps.location.isValid())
	{
		latitude = gps.location.lat();
		longitude = gps.location.lng();
		//	setTime(gps.time.hour(), gps.time.minute(), gps.time.second(), gps.date.day(), gps.date.month(), gps.date.year());
		tmElements_t tm;
		tm.Hour = gps.time.hour();
		tm.Minute = gps.time.minute();
		tm.Second = gps.time.second();
		tm.Day = gps.date.day();
		tm.Month = gps.date.month();
		tm.Year = gps.date.year();
		setTime(myTZ.toLocal(makeTime(tm)));
		showDateTime();
		return true;
	}
	else
		return false;
}

#endif