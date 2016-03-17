
/**
* Chicken Coop Door/Light controller for ESP8266 board.
* Copyright 2016, Ioan Ghip <ioanghip (at) gmail (dot) com>
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
* 02110-1301, USA.
*/

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266mDNS.h>
#include <TinyGPS++.h>            //https://github.com/mikalhart/TinyGPSPlus
#include <SoftwareSerial.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <TimeLord.h>             //https://github.com/probonopd/TimeLord
#include <TimeLib.h>              //https://github.com/PaulStoffregen/Time
#include <TimeAlarms.h>           //https://github.com/PaulStoffregen/TimeAlarms
#include <Timezone.h>             //https://github.com/schizobovine/Timezone (https://github.com/JChristensen/Timezone)
#include <EEPROM.h>               //http://playground.arduino.cc/Code/EEPROMWriteAnything
#include <Arduino.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>


// we don't need to send message to serial if not connected to a computer. Comment the following line if not debuging
#define __DEBUG__

#if defined (__DEBUG__)
#define DEBUG_begin(val) Serial.begin(val)
#define DEBUG_print(val) Serial.print(val)
#define DEBUG_println(val) Serial.println(val)
#else
#define DEBUG_begin(val)
#define DEBUG_print(val)
#define DEBUG_println(val)
#endif

#define COOP_VERSION "7"

const String HTTP_HEAD_COOP = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/><title>Chicken Coop</title>";
const String HTTP_STYLE_COOP = "<style>div,input{padding:5px;font-size:1em;} input{width:95%;} body{text-align: center;} button{border:0;border-radius:0.3rem;background-color:#1fa3ec;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;} input[type=\"submit\"]{border:0;border-radius:0.3rem;background-color:#1fa3ec;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;}</style>";
const String HTTP_HEAD_END_COOP = "</head><body><div style='font-family: verdana; text-align: left; display: inline-block;'><h1>Chicken Coop</h1>";
const String HTTP_END_COOP = "</div></body></html>";
const String HTTP_BUTTON_COOP = "<p><form action=\"{ACT}\" method=\"get\"><button>{TXT}</button></form></p>";
const String HTTP_LINK_CONFIG_COOP = "<p><a href=\"/RESET\" onclick=\"return confirm('Are you sure?');\">Configuration</a></p>";
const String HTTP_EDIT_COOP = "<p><label for=\"{ENAME}\">{LABEL}</label><input autocorrect=\"off\" autocapitalize=\"off\" spellcheck=\"false\" type=\"{ETYPE}\" name=\"{ENAME}\" value=\"{EVALUE}\"</input></p>";
const String HTTP_FORM_SAVE_COOP = "<p><form action=\"/SAVE\" method=\"post\">{FIELDS}<p><input style=button type=\"submit\" value=\"Save settings\"></p></form></p>";
const String HTTP_BEGIN_COOP = HTTP_HEAD_COOP + HTTP_STYLE_COOP + HTTP_HEAD_END_COOP;

const char* serverOTAIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input style=button type='file' name='update'><input style=button type='submit' value='Update'></form>";

// dynamic dns
const String DYNDNS = "http://{DYNDNSIP}/nic/update?hostname={DYNDNSDOMAIN}&myip={IP}";

struct dyndns_t
{
	char dyndnsserver[50] = "dynupdate.no-ip.com"; // dyndns provider domain
	char dyndnsname[50] = ""; // dyndns personal domain
	char dyndnsuser[50] = ""; // dyndns user name
	char dyndnspass[50] = ""; // dyndns password
} configuration;


int door_relay = 2; // gpo2 - d4
int light_relay = 14; // gop14 - d5

					  // what is our longitude (west values negative) and latitude (south values negative)
float latitude = 44.9308; // if you use a GPS to sync time, the value is going to be replaced by the GPS 
float longitude = -123.0289;

// we will syncronize the time with an GPS connected tot pin gpo13 -> gps TX, gpo15 -> gpx RX
static const int RXPin = 13; // gpo13 - d7
static const int TXPin = 15; // gpo15 - d8
							 // change the speed to match your GPS 
static const uint32_t GPSBaud = 38400;

SoftwareSerial SerialGPS = SoftwareSerial(RXPin, TXPin);
TinyGPSPlus gps;

// using NTP to retrieve the local time
const char* ntpServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
unsigned int localPort = 2390;      // local port to listen for UDP packets

									// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;

//Australia Eastern Time Zone (Sydney, Melbourne)
TimeChangeRule aEDT = { "AEDT", First, Sun, Oct, 2, 660 };    //UTC + 11 hours
TimeChangeRule aEST = { "AEST", First, Sun, Apr, 3, 600 };    //UTC + 10 hours
Timezone ausET(aEDT, aEST);

//Central European Time (Frankfurt, Paris)
TimeChangeRule CEST = { "CEST", Last, Sun, Mar, 2, 120 };     //Central European Summer Time
TimeChangeRule CET = { "CET ", Last, Sun, Oct, 3, 60 };       //Central European Standard Time
Timezone CE(CEST, CET);

//United Kingdom (London, Belfast)
TimeChangeRule BST = { "BST", Last, Sun, Mar, 1, 60 };        //British Summer Time
TimeChangeRule GMT = { "GMT", Last, Sun, Oct, 2, 0 };         //Standard Time
Timezone UK(BST, GMT);

//US Eastern Time Zone (New York, Detroit)
TimeChangeRule usEDT = { "EDT", Second, Sun, Mar, 2, -240 };  //Eastern Daylight Time = UTC - 4 hours
TimeChangeRule usEST = { "EST", First, Sun, Nov, 2, -300 };   //Eastern Standard Time = UTC - 5 hours
Timezone usET(usEDT, usEST);

//US Central Time Zone (Chicago, Houston)
TimeChangeRule usCDT = { "CDT", Second, dowSunday, Mar, 2, -300 };
TimeChangeRule usCST = { "CST", First, dowSunday, Nov, 2, -360 };
Timezone usCT(usCDT, usCST);

//US Mountain Time Zone (Denver, Salt Lake City)
TimeChangeRule usMDT = { "MDT", Second, dowSunday, Mar, 2, -360 };
TimeChangeRule usMST = { "MST", First, dowSunday, Nov, 2, -420 };
Timezone usMT(usMDT, usMST);

//Arizona is US Mountain Time Zone but does not use DST
Timezone usAZ(usMST, usMST);

//US Pacific Time Zone (Portland, Salem - OR, Las Vegas, Los Angeles)
TimeChangeRule usPDT = { "PDT", Second, dowSunday, Mar, 2, -420 }; //Daylight time = UTC - 7 hours
TimeChangeRule usPST = { "PST", First, dowSunday, Nov, 2, -480 }; //Standard time = UTC - 8 hours
Timezone usPT(usPDT, usPST);

//Change here the region you are in. I'm in US Pacific Time Zone
Timezone myTZ = usPT;
//pointer to the time change rule, use to get the TZ offset
TimeChangeRule *tcr;

byte doorOpeningHour;
byte doorOpeningMinute;
byte doorClosingHour;
byte doorClosingMinute;
byte ligthOnHour;
byte ligthOnMinute;
byte lightOffHour;
byte lightOffMinute;

boolean isDoorOpen = true;
boolean isLightOn = true;

boolean isAP = false;

/* Set these to your desired credentials. */
const char *ssid = "ChickenCoop";
const char *password = "12345678";
const char *host = "coop";


ESP8266WebServer webServer(80);

template <class T> int EEPROM_writeAnything(int ee, const T& value);
template <class T> int EEPROM_readAnything(int ee, T& value);

void setup()
{
	pinMode(door_relay, OUTPUT);
	pinMode(light_relay, OUTPUT);

	digitalWrite(door_relay, LOW);
	digitalWrite(light_relay, HIGH);

	// init serial for time sync with the GPS
	SerialGPS.begin(GPSBaud);

	// init serial for debugging
	DEBUG_begin(115200);
	DEBUG_print("Chicken Coop v");
	DEBUG_println(COOP_VERSION);

	// init the eeprom
	EEPROM.begin(512);

	String ip;
	isAP = isAPFlag();

	if (isAP)
	{
		/* You can remove the password parameter if you want the AP to be open. */
		WiFi.softAP(ssid, password);
		DEBUG_print("AP Mode, IP address: ");
	}
	else
	{
		//WiFiManager: Local intialization. Once its business is done, there is no need to keep it around
		WiFiManager wifi;
		//set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
		wifi.setAPCallback(configModeCallback);
		if (!wifi.autoConnect("ChickenCoop"))
		{
			DEBUG_println("failed to connect and hit timeout");
			//reset and try again, or maybe put it to deep sleep
			ESP.reset();
			Alarm.delay(1000);
		}
		while (WiFi.waitForConnectResult() != WL_CONNECTED)
		{
			DEBUG_println("Connection Failed! Rebooting...");
			delay(5000);
			ESP.restart();
		}
		//if you get here you have connected to the WiFi
		DEBUG_print("WiFi Mode, IP address: ");
	}

	DEBUG_println(WiFi.localIP());
	MDNS.begin(host);

	// Match the request for the web server
	webServer.on("/", mainHTMLPage);
	webServer.on("/ACT=OPEN", openDoor);
	webServer.on("/ACT=CLOSE", closeDoor);
	webServer.on("/ACT=LON", lightOn);
	webServer.on("/ACT=LOFF", lightOff);
	webServer.on("/RESET", resetToAPHTMLPage);
	webServer.on("/AP", doAPHTMLPage);
	webServer.on("/WIFI", doWiFiHTMLPage);
	webServer.on("/DYNDNS", doDynDNSHTMLPage);
	webServer.on("/SAVE", HTTP_POST, []()
	{
		webServer.arg("dyndnsserver").toCharArray(configuration.dyndnsserver, 50);
		webServer.arg("dyndnsname").toCharArray(configuration.dyndnsname, 50);
		webServer.arg("dyndnsuser").toCharArray(configuration.dyndnsuser, 50);
		webServer.arg("dyndnspass").toCharArray(configuration.dyndnspass, 50);
		// save new values
		saveDynDNStoEEPROM();
		// update DNS
		dynDNS();
		// server the main page
		mainHTMLPage();
	});

	webServer.on("/reboot", HTTP_GET, []()
	{
		webServer.send(200, "text/html", "Doesn't work yet");
		//ESP.restart();
	});

	// web firmware update - start
	webServer.on("/firmware", HTTP_GET, []()
	{
		webServer.sendHeader("Connection", "close");
		webServer.sendHeader("Access-Control-Allow-Origin", "*");
		webServer.send(200, "text/html", serverOTAIndex);
	});

	// OTA from web post
	webServer.on("/update", HTTP_POST, []() {
		webServer.sendHeader("Connection", "close");
		webServer.sendHeader("Access-Control-Allow-Origin", "*");
		webServer.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
		ESP.restart();
	}, []() {
		HTTPUpload& upload = webServer.upload();
		if (upload.status == UPLOAD_FILE_START) {
			Serial.setDebugOutput(true);
			WiFiUDP::stopAll();
			DEBUG_print("Update: ");
			DEBUG_println(upload.filename.c_str());
			uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
			if (!Update.begin(maxSketchSpace)) {//start with max available size
				Update.printError(Serial);
			}
		}
		else if (upload.status == UPLOAD_FILE_WRITE) {
			if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
				Update.printError(Serial);
			}
		}
		else if (upload.status == UPLOAD_FILE_END) {
			if (Update.end(true)) { //true to set the size to the current progress
				DEBUG_println("Update Success: Rebooting...");
			}
			else {
				Update.printError(Serial);
			}
			Serial.setDebugOutput(false);
		}
		yield();
	});
	// web firmware update - end

	// Start the web server
	webServer.begin();
	MDNS.addService("http", "tcp", 80);

	DEBUG_print("Ready! Open http://");
	DEBUG_print(host);
	DEBUG_println(".local in your browser");

	// Starting UDP
	udp.begin(localPort);

	// wait for GPS fix and then set the date/time
	setTheClock();

	// create the alarms 
	// schedule alarms for today, do it once now
	Alarm.timerOnce(5, setAllAlarmsForTheDay);
	// every hour, sync the clock
	Alarm.timerRepeat(60 * 60, setTheClock);
	// recalculate times and schedule alarms for the day. Do it around middle of the night
	Alarm.alarmRepeat(1, 30, 0, setAllAlarmsForTheDay);

	// if we aren't an AP, do & schedule update with dyndns, enable OTA
	if (!isAP)
	{
		// allow OTA update from inside Arduino IDE
		enableOTAfromIDE();
		// Update IP on Dynamic DNS
		dynDNS();
		// once a day, at 2AM update the IP with the dyndns
		Alarm.alarmRepeat(2, 00, 0, dynDNS);
	}
}

void loop()
{
	if (!isAP)
	{
		ArduinoOTA.handle();
	}
	webServer.handleClient();
	Alarm.delay(0);
}

String makeHTMLButton(String action, String text)
{
	String btn = HTTP_BUTTON_COOP;
	btn.replace("{ACT}", action);
	btn.replace("{TXT}", text);
	return btn;
}


String makeHTMLEdit(String label, String etype, String ename, String evalue)
{
	String edit = HTTP_EDIT_COOP;
	edit.replace("{LABEL}", label);
	edit.replace("{ETYPE}", etype);
	edit.replace("{ENAME}", ename);
	edit.replace("{EVALUE}", evalue);
	return edit;
}

void mainHTMLPage()
{
	String page = HTTP_BEGIN_COOP;

	page += "The time is: " + String(hour()) + ":" + String(niceMinuteSecond(minute())) + ", " + String(month()) + "/" + String(day()) + "/" + String(year()) + "<br>";
	page += "Door opening time: " + String(doorOpeningHour) + ":" + String(niceMinuteSecond(doorOpeningMinute)) + "<br>";
	page += "Door closing time: " + String(doorClosingHour) + ":" + String(niceMinuteSecond(doorClosingMinute)) + "<br>";
	page += "Light ON time: " + String(ligthOnHour) + ":" + String(niceMinuteSecond(ligthOnMinute)) + "<br>";
	page += "Light OFF time: " + String(lightOffHour) + ":" + String(niceMinuteSecond(lightOffMinute)) + "<br>";

	if (isDoorOpen)
	{
		page += makeHTMLButton("/ACT=CLOSE", "Close door");
	}
	else
	{
		page += makeHTMLButton("/ACT=OPEN", "Open door");
	}

	if (isLightOn)
	{
		page += makeHTMLButton("/ACT=LOFF", "Light off");
	}
	else
	{
		page += makeHTMLButton("/ACT=LON", "Light on");
	}

	page += HTTP_LINK_CONFIG_COOP;
	// bottom of the html page
	page += HTTP_END_COOP;
	webServer.send(200, "text/html", page);
}

void resetToAPHTMLPage()
{
	String page = HTTP_BEGIN_COOP;
	page += "Do you want to run as AP or<br>do you want to connect to a WiFi network?";
	page += makeHTMLButton("/AP", "Make AP");
	page += makeHTMLButton("/WIFI", "Connect to WiFi");
	page += makeHTMLButton("/DYNDNS", "Dynamic DNS");
	page += "<br>";
	page += makeHTMLButton("/firmware", "Update firmware");
	page += makeHTMLButton("/reboot", "Reboot");
	page += HTTP_END_COOP;
	webServer.send(200, "text/html", page);
}

void doAPHTMLPage()
{
	setAPFlag();
	String page = HTTP_BEGIN_COOP;
	page += "Creating AP and rebooting...";
	page += HTTP_END_COOP;
	webServer.send(200, "text/html", page);
	WiFiManager wifi;
	DEBUG_println(wifi.getConfigPortalSSID());
	wifi.resetSettings();
	ESP.reset();
	Alarm.delay(5000);
}

void doWiFiHTMLPage()
{
	unsetAPFlag();
	String page = HTTP_BEGIN_COOP;
	page += "Creating WiFi and rebooting...";
	page += HTTP_END_COOP;
	WiFiManager wifi;
	DEBUG_println(wifi.getConfigPortalSSID());
	wifi.resetSettings();
	ESP.reset();
	Alarm.delay(5000);
}

void doDynDNSHTMLPage()
{
	readDynDNSfromEEPROM();
	String page = HTTP_BEGIN_COOP;
	page += "Please enter your no-ip info:";
	page += HTTP_FORM_SAVE_COOP;
	String formFields;
	formFields = makeHTMLEdit("Server ", "text", "dyndnsserver", String(configuration.dyndnsserver));
	formFields += makeHTMLEdit("Name ", "text", "dyndnsname", String(configuration.dyndnsname));
	formFields += makeHTMLEdit("Login ", "text", "dyndnsuser", String(configuration.dyndnsuser));
	formFields += makeHTMLEdit("Password ", "password", "dyndnspass", String(configuration.dyndnspass));
	page.replace("{FIELDS}", formFields);
	page += HTTP_END_COOP;
	webServer.send(200, "text/html", page);
}

void readDynDNSfromEEPROM()
{
	DEBUG_println("read dyndns eeprom");
	EEPROM_readAnything(50, configuration);
}

void saveDynDNStoEEPROM()
{
	// we write the dyndns name, login, password to eeprom
	// position 20, 21, 22 holds the size of each string, position 23 has the first field
	DEBUG_println("save dyndns eeprom");	
	EEPROM_writeAnything(50, configuration);
}


void configModeCallback(WiFiManager *myWiFiManager)
{
	DEBUG_println("Entered config mode");
	DEBUG_println(WiFi.softAPIP());
}

// check if val is in the time period between start and end
boolean inTimePeriod(time_t val, byte startHour, byte startMinute, byte endHour, byte endMinute)
{
	// to check if we are in a certain time period, we convert 
	// the begining and ending of the period to unix time and then 
	// check the if begin <= now() <= end
	time_t starttm, endtm;

	tmElements_t tm;
	tm.Second = 0;
	tm.Day = day();
	tm.Month = month();
	tm.Year = year() - 1970;

	tm.Hour = startHour;
	tm.Minute = startMinute;
	starttm = makeTime(tm);

	tm.Hour = endHour;
	tm.Minute = endMinute;
	endtm = makeTime(tm);

	return (val >= starttm) && (now() <= endtm);
}
boolean isDoorOpenPeriod()
{
	return inTimePeriod(now(), doorOpeningHour, doorOpeningMinute, doorClosingHour, doorClosingMinute);
}

boolean isLightONPeriod()
{
	return inTimePeriod(now(), ligthOnHour, ligthOnMinute, lightOffHour, lightOffMinute);
}

void setAllAlarmsForTheDay()
{
	calculateTodaysSunriseSunset();

	// if at this time, the door needs to be open, open it, else. close it. Same for the light
	if (isDoorOpenPeriod())
	{
		openDoor();
	}
	else
	{
		closeDoor();
	}

	// is it light on time?
	if (isLightONPeriod())
	{
		lightOn();
	}
	else
	{
		lightOff();
	}

	// open the door in the morning
	showATime(doorOpeningHour, doorOpeningMinute);
	Alarm.alarmOnce(doorOpeningHour, doorOpeningMinute, 0, openDoor);
	// close the door at night
	showATime(doorClosingHour, doorClosingMinute);
	Alarm.alarmOnce(doorClosingHour, doorClosingMinute, 0, closeDoor);
	// create the alarm to turn the light on before sunset 
	Alarm.alarmOnce(ligthOnHour, ligthOnMinute, 0, lightOn);
	// ...and off after door closing
	Alarm.alarmOnce(lightOffHour, lightOffMinute, 0, lightOff);
}

void showDateTime()
{
	// digital clock display of the time
	DEBUG_print(hour());
	DEBUG_print(":");
	DEBUG_print(niceMinuteSecond(minute()));
	DEBUG_print(":");
	DEBUG_print(niceMinuteSecond(second()));
	DEBUG_print(" ");
	DEBUG_print(day());
	DEBUG_print("/");
	DEBUG_print(month());
	DEBUG_print("/");
	DEBUG_println(year());
}

void showATime(int h, int m)
{
	DEBUG_print(h);
	DEBUG_print(":");
	DEBUG_println(niceMinuteSecond(m));
}

void calculateTodaysSunriseSunset()
{
	// TimeLord library for calculating sunset / sunrise times
	TimeLord sunrisesunset;

	//Time Lord constants
	//tl_second=0, tl_minute=1, tl_hour=2, tl_day=3, tl_month=4, tl_year=5
	// what TimeLord lib returns will be stored here:
	byte todaySunRiseHour;
	byte todaySunRiseMinute;
	byte todaySunSetHour;
	byte todaySunSetMinute;


	//extract the timezone tcr->offset
	time_t utc, local;
	utc = myTZ.toUTC(now());
	local = myTZ.toLocal(utc, &tcr);
	// tcr->offset has the time zone
	sunrisesunset.TimeZone(tcr->offset);
	sunrisesunset.DstRules(3, 2, 11, 1, 60); // USA
	sunrisesunset.Position(latitude, longitude);
	byte rise[] = { 0, 0, 12, (byte)day(), (byte)month(), (byte)year() };
	byte set[] = { 0, 0, 12, (byte)day(), (byte)month(), (byte)year() };
	sunrisesunset.SunRise(rise);
	todaySunRiseHour = rise[tl_hour];
	todaySunRiseMinute = rise[tl_minute];
	sunrisesunset.SunSet(set);
	todaySunSetHour = set[tl_hour];
	todaySunSetMinute = set[tl_minute];

	// calculate the opening/closing/light  on/off times
	doorOpeningHour = todaySunRiseHour;
	doorOpeningMinute = todaySunRiseMinute;
	doorClosingHour = todaySunSetHour + 1;
	doorClosingMinute = todaySunSetMinute;
	ligthOnHour = todaySunSetHour - 1;
	ligthOnMinute = todaySunSetMinute;
	lightOffHour = todaySunSetHour + 2;
	lightOffMinute = todaySunSetMinute;
}


String niceMinuteSecond(int m)
{
	char sz[4];
	sprintf(sz, "%02d", m);
	return String(sz);
}

void lightOn()
{
	digitalWrite(light_relay, HIGH);
	isLightOn = true;
	mainHTMLPage();
}

void lightOff()
{
	digitalWrite(light_relay, LOW);
	isLightOn = false;
	mainHTMLPage();
}

void openDoor()
{
	DEBUG_println("Opening door...");
	showDateTime();
	digitalWrite(door_relay, LOW);
	isDoorOpen = true;
	mainHTMLPage();
}

void closeDoor()
{
	DEBUG_println("Closing door...");
	showDateTime();
	digitalWrite(door_relay, HIGH);
	isDoorOpen = false;
	mainHTMLPage();
}


void setAPFlag()
{
	EEPROM_writeAnything(20, true);
}

void unsetAPFlag()
{
	EEPROM_writeAnything(20, false);
}

boolean isAPFlag()
{
	boolean ap;
	EEPROM_readAnything(20, ap);
	return ap;
}

template <class T> int EEPROM_writeAnything(int ee, const T& value)
{
	const byte* p = (const byte*)(const void*)&value;
	unsigned int i;
	for (i = 0; i < sizeof(value); i++)
		EEPROM.write(ee++, *p++);
	EEPROM.commit();
	return i;
}

template <class T> int EEPROM_readAnything(int ee, T& value)
{
	byte* p = (byte*)(void*)&value;
	unsigned int i;
	for (i = 0; i < sizeof(value); i++)
		*p++ = EEPROM.read(ee++);
	return i;
}


void dynDNS()
{
	// put the IP on a DYNDNS for easy access
	HTTPClient http;
	String dyndns = DYNDNS;
	IPAddress dyndnsServerIP;
	readDynDNSfromEEPROM();
	WiFi.hostByName(configuration.dyndnsserver, dyndnsServerIP);
	dyndns.replace("{DYNDNSIP}", dyndnsServerIP.toString());
	dyndns.replace("{DYNDNSDOMAIN}", String(configuration.dyndnsname));
	dyndns.replace("{IP}", WiFi.localIP().toString());
	DEBUG_println(dyndns);
	http.begin(dyndns); //HTTP		
	http.setAuthorization(configuration.dyndnsuser, configuration.dyndnspass);
	int httpCode = http.GET();
	DEBUG_println(httpCode);
	if (httpCode == HTTP_CODE_OK)
	{
		String payload = http.getString();
		DEBUG_println(payload);
	}
}

boolean syncTimeWithGPSorNTP()
{
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
	else if (!isAPFlag())
	{
		return getNTPUnixTime();
	}
	else
	{
		return false;
	}
}

void setTheClock()
{
	DEBUG_println("Please wait, syncing the clock with GPS...");
	while (!syncTimeWithGPSorNTP())
	{
		delay(10);
	}
}

boolean getNTPUnixTime()
{
	// time.nist.gov NTP server address
	IPAddress timeServerIP; 
	//get a random server from the pool
	WiFi.hostByName(ntpServerName, timeServerIP);
	// send an NTP packet to a time server
	sendNTPpacket(timeServerIP);
	// wait to see if a reply is available							 
	delay(1000);

	int cb = udp.parsePacket();
	if (!cb)
	{
		DEBUG_println("no packet yet");
		return false;
	}
	else
	{
		// We've received a packet, read the data from it
		udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
	    //the timestamp starts at byte 40 of the received packet and is four bytes,
		// or two words, long. First, extract the two words:
		unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
		unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
		// combine the four bytes (two words) into a long integer
		// this is NTP time (seconds since Jan 1 1900):
		unsigned long secsSince1900 = highWord << 16 | lowWord;
		// now convert NTP time into everyday time:
		// Unix time starts on Jan 1 1970. In seconds, that's 2208988800:
		const unsigned long seventyYears = 2208988800UL;
		// subtract seventy years:
		unsigned long epoch = secsSince1900 - seventyYears;
		//setTime(epoch);
		setTime(myTZ.toLocal(epoch));
		DEBUG_print("Local date/time: ");
		showDateTime();
		return true;
	}
}

// send an NTP request to the time server at the given address
unsigned long sendNTPpacket(IPAddress& address)
{
	Serial.println("Please wait, syncing the clock with NTP...");
	// set all bytes in the buffer to 0
	memset(packetBuffer, 0, NTP_PACKET_SIZE);
	// Initialize values needed to form NTP request
	// (see URL above for details on the packets)
	packetBuffer[0] = 0b11100011;   // LI, Version, Mode
	packetBuffer[1] = 0;     // Stratum, or type of clock
	packetBuffer[2] = 6;     // Polling Interval
	packetBuffer[3] = 0xEC;  // Peer Clock Precision
							 // 8 bytes of zero for Root Delay & Root Dispersion
	packetBuffer[12] = 49;
	packetBuffer[13] = 0x4E;
	packetBuffer[14] = 49;
	packetBuffer[15] = 52;
	// all NTP fields have been given values, now
	// you can send a packet requesting a timestamp:
	udp.beginPacket(address, 123); //NTP requests are to port 123
	udp.write(packetBuffer, NTP_PACKET_SIZE);
	udp.endPacket();
}


// this is used for programming from inside Arduino IDE
void enableOTAfromIDE()
{
	// Hostname defaults to esp8266-[ChipID]
	ArduinoOTA.setHostname("coop-esp8266");
	// No authentication by default
	//ArduinoOTA.setPassword((const char *)"123");
	ArduinoOTA.onStart([]()
	{
		DEBUG_println("Start");
	});
	ArduinoOTA.onEnd([]()
	{
		DEBUG_println("End");
	});
	ArduinoOTA.onError([](ota_error_t error)
	{
		DEBUG_print("Error: ");
		DEBUG_println(error);
		if (error == OTA_AUTH_ERROR) DEBUG_println("Auth Failed");
		else if (error == OTA_BEGIN_ERROR) DEBUG_println("Begin Failed");
		else if (error == OTA_CONNECT_ERROR) DEBUG_println("Connect Failed");
		else if (error == OTA_RECEIVE_ERROR) DEBUG_println("Receive Failed");
		else if (error == OTA_END_ERROR) DEBUG_println("End Failed");
	});
	ArduinoOTA.begin();
}
