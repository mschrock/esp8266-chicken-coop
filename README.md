# Chicken Coop Door 
## powered by esp8266

The controller will sync the date/time from a GPS or using a NTP server. If you have a GPS, the controller will get the coordinates of the place to calculate the sunrise and sunset.

The door opens automatically at sunrise and closes one hour after the sunset. 


## Code 

If you use a GPS, the coordinates will be updated automatically The coordinates are used to calculate the sunrise/sunset time. If you are not using a gps, replace the values of: 

float latitude = 44.9308;
float longitude = -123.0289;

with your own values. 

At first boot, you'll have to connect to the Access Point created by the esp8266 (go to 192.168.4.1) and configure your wifi network.

After esp8266 connects to your wifi, open in your browser the IP assigned, for the available commands.

![Main Page](http://i.imgur.com/es0Xqiv.jpg)

## Wifi 

The code allows the "coop door" to run connected to your wifi, or as stand alone in AP mode.

![Wifi Mode](http://i.imgur.com/mbSJfk7.jpg)

##DynDNS

![DynDNS](http://i.imgur.com/iMRAzJt.jpg)



