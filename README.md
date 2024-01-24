# Victron-VE.Bus---esp32
Victron Multiplus2 VE.Bus connected to Esp32

This is derived from:
https://github.com/pv-baxi/esp32ess

It implements net-zero with help of a shelly 3EM and a multiplus2 48/3000
hardware is a esp32 + RS485 converter + 12V to 5V buck converter.

Schematic:
![ESP32_VEBUS.png](ESP32_VEBUS.png "schematic")

You will have to modify the RS485 converter according to PV-Baxi instructions. Remove the resistors. 
The schematic for the original VE.Bus configuration ist this:
![schematic.jpg](schematic.jpg "schematic")
Yes, Vin is not a good place to get power, but it works.

This is "in progress" and i refuse any liability! DO NOT have the multiplus and the esp32 usb connected simultaneously! Program the esp32 alone, detach usb and only then connect to ve.bus. You also have to change the WiFi credentials and the shelly IP:

------------------------------------

//Wifi password

const char* SSID = "FSM";

const char* PASSWORD = "0101010101";

------------------------------------

Mine looks like this:
![IMG_20240121_022232.jpg](IMG_20240121_022232.jpg "gebastel")

