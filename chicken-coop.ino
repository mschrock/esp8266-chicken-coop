
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
#include <DNSServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <TimeLord.h>             //https://github.com/probonopd/TimeLord
#include <TimeLib.h>              //https://github.com/PaulStoffregen/Time
#include <TimeAlarms.h>           //https://github.com/PaulStoffregen/TimeAlarms
#include <Timezone.h>             //https://github.com/schizobovine/Timezone (https://github.com/JChristensen/Timezone)
#include <EEPROM.h>               //http://playground.arduino.cc/Code/EEPROMWriteAnything
#include <Arduino.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Wire.h>

// to enable deep sleep uncomment the following line
#define __SLEEP_MODE__  

// if you want to get time from a GPS, uncomment following line
//#define __GPS_INSTALLED__  

// if you want to use LCD
//#define __LCD_INSTALLED__

#define __RTC_INSTALLED__

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


const String HTTP_HEAD_EXPIRE = "<meta http - equiv = \"cache-control\" content=\"max-age=0\" /><meta http-equiv=\"cache-control\" content=\"no-cache\" /><meta http-equiv=\"expires\" content=\"0\" /><meta http-equiv=\"expires\" content=\"Tue, 01 Jan 1980 1:00:00 GMT\" /><meta http-equiv=\"pragma\" content=\"no-cache\" />";
const String HTTP_HEAD_COOP = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/>" + HTTP_HEAD_EXPIRE + "<title>Chicken Coop</title>";
const String HTTP_STYLE_COOP = "<style>div,input{padding:5px;font-size:1em;} input{width:95%;} body{text-align: center;} button{border:0;border-radius:0.3rem;background-color:#1fa3ec;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;} input[type=\"submit\"]{border:0;border-radius:0.3rem;background-color:#1fa3ec;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;}</style>";
const String HTTP_HEAD_END_COOP = "</head><body><div style='font-family: verdana; text-align: left; display: inline-block;'><h1>Chicken Coop</h1>";
const String HTTP_END_COOP = "</div></body></html>";
const String HTTP_BUTTON_COOP = "<p><form action=\"{ACT}\" method=\"get\"><button>{TXT}</button></form></p>";
const String HTTP_LINK_CONFIG_COOP = "<p><a href=\"/RESET\" onclick=\"return confirm('Are you sure?');\">Configuration</a></p>";
const String HTTP_EDIT_COOP = "<p><label for=\"{ENAME}\">{LABEL}</label><input autocorrect=\"off\" autocapitalize=\"off\" spellcheck=\"false\" type=\"{ETYPE}\" name=\"{ENAME}\" value=\"{EVALUE}\"</input></p>";
const String HTTP_FORM_SAVE_COOP = "<p><form action=\"/SAVE\" method=\"post\">{FIELDS}<p><input style=button type=\"submit\" value=\"Save settings\"></p></form></p>";
const String HTTP_BEGIN_COOP = HTTP_HEAD_COOP + HTTP_STYLE_COOP + HTTP_HEAD_END_COOP;
const String HTTP_CAMERA = "<p><IMG SRC=\"http://192.168.1.61/axis-cgi/mjpg/video.cgi?resolution=352x240\" ALT=\"Live Image\"><p>";
const char* serverOTAIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input style=button type='file' name='update'><input style=button type='submit' value='Update'></form>";

// dynamic dns
const String DYNDNS = "http://{DYNDNSIP}/nic/update?hostname={DYNDNSDOMAIN}&myip={IP}";

#define COOP_VERSION "V9"

struct dyndns_t
{
	char configured[10] = COOP_VERSION;
	char dyndnsserver[50] = "dynupdate.no-ip.com"; // dyndns provider domain
	char dyndnsname[50] = "yourname.no-ip.org"; // dyndns personal domain
	char dyndnsuser[50] = "username"; // dyndns user name
	char dyndnspass[50] = "password"; // dyndns password
	boolean isAP = false;
} configuration;


int door_relay1 = 12; // gpo4 - d2
int door_relay2 = 14; // gpo14 - d5

// what is our longitude (west values negative) and latitude (south values negative)
float latitude = 44.9308; // if you use a GPS to sync time, the value is going to be replaced by the GPS 
float longitude = -123.0289;

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

// Set these to your desired credentials. 
const char *ssid = "ChickenCoop";
const char *password = "12345678";
const char *host = "coop";

const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 4, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

template <class T> int EEPROM_writeAnything(int ee, const T& value);
template <class T> int EEPROM_readAnything(int ee, T& value);

void setup()
{

	Wire.begin();

	String ip;

	pinMode(door_relay1, OUTPUT);
	pinMode(door_relay2, OUTPUT);

	digitalWrite(door_relay1, LOW);
	digitalWrite(door_relay2, LOW);

#if defined (__GPS_INSTALLED__)
	// init serial for time sync with the GPS
	initGPS();

#endif

	// init serial for debugging
	DEBUG_begin(115200);
	DEBUG_print("Chicken Coop ");
	DEBUG_println(COOP_VERSION);

	// init the eeprom
	EEPROM.begin(512);


	// initial configuration
	readConfigFromEEPROM();
	if (!(String(configuration.configured) == String(COOP_VERSION)))
	{
		String(COOP_VERSION).toCharArray(configuration.configured, 10);
		String("dynupdate.no-ip.com").toCharArray(configuration.dyndnsserver, 50); // dyndns provider domain
		String("yourname.no-ip.org").toCharArray(configuration.dyndnsname, 50); // dyndns personal domain
		String("username").toCharArray(configuration.dyndnsuser, 50); // dyndns user name
		String("password").toCharArray(configuration.dyndnspass, 50); // dyndns password
		configuration.isAP = false;
		saveConfigToEEPROM();
	}

	if (configuration.isAP)
	{

		WiFi.mode(WIFI_AP);
		WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
		WiFi.softAP(ssid, password);

		// if DNSServer is started with "*" for domain name, it will reply with
		// provided IP to all DNS request
		dnsServer.start(DNS_PORT, "*", apIP);

		// You can remove the password parameter if you want the AP to be open.
		
		DEBUG_print("AP Mode, IP address: ");
		DEBUG_println(apIP);
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
			delay(1000);
		}
		while (WiFi.waitForConnectResult() != WL_CONNECTED)
		{
			DEBUG_println("Connection Failed! Rebooting...");
			delay(5000);
			ESP.restart();
		}
		//if you get here you have connected to the WiFi
		DEBUG_print("WiFi Mode, IP address: ");
		DEBUG_println(WiFi.localIP());
	}
	

	// Match the request for the web server
	webServer.onNotFound(mainHTMLPage);
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
		saveConfigToEEPROM();
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

#if defined (__RTC_INSTALLED__)
	setSyncProvider(rtc_get);   // the function to get the time from the RTC

	if (timeStatus() != timeSet)
		Serial.println("Unable to sync with the RTC");
	else
		Serial.println("RTC has set the system time");
#endif

	// create the alarms 
	// every 5 min, check if the door is in the right position
	Alarm.timerRepeat((5 * 60), handleDoorAndLight);
	// schedule alarms for today, do it once now
	Alarm.timerOnce(5, setAllAlarmsForTheDay);
	// recalculate times and schedule alarms for the day. Do it around middle of the night
	Alarm.alarmRepeat(1, 0, 0, setAllAlarmsForTheDay);

#if defined (__SLEEP_MODE__)   	
	// put wifi to sleep at night
	Alarm.alarmRepeat(22, 30, 0, forceSleepON); 
	Alarm.alarmRepeat(4, 00, 0, forceSleepOFF);
	// put wifi to sleep when nobody is at home
	Alarm.alarmRepeat(9, 30, 0, forceSleepON);
	Alarm.alarmRepeat(15, 00, 0, forceSleepOFF);
#endif

	// if we aren't an AP, do & schedule update with dyndns, enable OTA
	if (!configuration.isAP)
	{		
		// if connected to the internet, sync the RTC and local clock with NTP
		initNTP();
		setTheClock();

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
	if (!configuration.isAP)
	{
		ArduinoOTA.handle();
	}
	dnsServer.processNextRequest();
	webServer.handleClient();	
	Alarm.delay(0);
}

void forceSleepON()
{
	DEBUG_println("Sleep... ");
	WiFi.forceSleepBegin();
}

void forceSleepOFF()
{
	DEBUG_println("Wake... ");
	WiFi.forceSleepWake();
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
	page += "The door is now: <b>" + (isDoorOpen ? String("OPEN") : String("CLOSED")) + "</b><br>";
	page += "Door opening time: " + String(doorOpeningHour) + ":" + String(niceMinuteSecond(doorOpeningMinute)) + "<br>";
	page += "Door closing time: " + String(doorClosingHour) + ":" + String(niceMinuteSecond(doorClosingMinute)) + "<br>";
//	page += "Light ON time: " + String(ligthOnHour) + ":" + String(niceMinuteSecond(ligthOnMinute)) + "<br>";
//	page += "Light OFF time: " + String(lightOffHour) + ":" + String(niceMinuteSecond(lightOffMinute)) + "<br>";
	page += "Temperature: " + String(rtc_getTemp()) + " &deg;C, " + String(rtc_getTemp() * 1.8 + 32) + " &deg;F" +"<br>";

	if (isDoorOpen)
	{
		page += makeHTMLButton("/ACT=CLOSE", "Close door");
	}
	else
	{
		page += makeHTMLButton("/ACT=OPEN", "Open door");
	}
/*
	if (isLightOn)
	{
		page += makeHTMLButton("/ACT=LOFF", "Light off");
	}
	else
	{
		page += makeHTMLButton("/ACT=LON", "Light on");
	}
*/
//	page += HTTP_CAMERA;
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
	readConfigFromEEPROM();
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

void readConfigFromEEPROM()
{
	DEBUG_println("read dyndns eeprom");
	EEPROM_readAnything(50, configuration);
}

void saveConfigToEEPROM()
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

	return (val >= starttm) && (val <= endtm);
}
boolean isDoorOpenPeriod()
{
	return inTimePeriod(now(), doorOpeningHour, doorOpeningMinute, doorClosingHour, doorClosingMinute);
}

boolean isLightONPeriod()
{
	return inTimePeriod(now(), ligthOnHour, ligthOnMinute, lightOffHour, lightOffMinute);
}

void handleDoorAndLight()
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
}

void setAllAlarmsForTheDay()
{
	handleDoorAndLight();

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
	isLightOn = true;
	mainHTMLPage();
}

void lightOff()
{
	isLightOn = false;
	mainHTMLPage();
}

void openDoor()
{
	DEBUG_println("Opening door...");
	showDateTime();
	digitalWrite(door_relay1, HIGH);
	Alarm.delay(100);
	digitalWrite(door_relay1, LOW);
	isDoorOpen = true;
	mainHTMLPage();
}

void closeDoor()
{
	DEBUG_println("Closing door...");
	showDateTime();
	digitalWrite(door_relay2, HIGH);
	Alarm.delay(100);
	digitalWrite(door_relay2, LOW);
	isDoorOpen = false;
	mainHTMLPage();
}


void setAPFlag()
{
	configuration.isAP = true;
	saveConfigToEEPROM();
}

void unsetAPFlag()
{
	configuration.isAP = false;
	saveConfigToEEPROM();
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
	readConfigFromEEPROM();
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
#if defined (__GPS_INSTALLED__) 
	if (getGPSUnixTime())
	{
		return true;
	}
	else
#endif
		if (!configuration.isAP)
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
	while (!syncTimeWithGPSorNTP())
	{
		delay(10);
	}
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

