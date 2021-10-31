# erichVK5-air-quality-monitor
Open hardware and software for air quality monitoring that leverages the arduino ecosystem.

The shield and accompanying software allow the CCS811 sensor to be used to monitor CO2 and total volatile organic compounds, and the AM2320 sensor allow humidity and temperature to be recorded as well.

The shield also has provision for a TM1367 based four digit LED display to allow the display of readings.

The PCB was designed in pcb-rnd and simplifies the connections for the TM1367 display, CCS811 sensor and AM2320 sensor to an underlying Arduino Uno R3.

In addition to taking and displaying readings every 10 seconds, the accompanying code will write time stamped readings to an SD card every three minutes, thanks to an Adafruit datalogging shield with a PCF8523 real time clock.

The hardware has been tested on the Adafruit "R3 compatible" version of their datalogging shield, and a genuine Arduino Uno R3.

The datalogging shield requires a CR1220 battery for the real time clock, and will require a FAT16 or FAT32 formatted SD card of up between 32MB and 32GB in size.

Afer assembling the datalogging shield's headers and inserting the battery, the shield should be mounted on the Arduino Uno R3 and the clock example code used to set the current time.

Once the air quality PCB has been assembled, it can be mounted on the datalogging shield, and the sketch uploaded.

After powering on, if all goes well, an initial flash screen "LLS" shuld appear, after which reading will be displayed sequentially, with new readings being taken every 10 seconds. Every three minites, cached readings are written to the SD card.

The unit should be powered down before inserting or removing the SD card.

Data logs are written in CSV format, with sequential naming starting with LLSAQM01.CSV

Temperature readings in Centigrade are preceded by "t" on the LED display
Humidity readings in percent are preceded by "h" on the LED display
Carbon dioxide readings in ppm are preceded by "CO2" on the LED display
Total volatile organic compound readings in ppm are preceded by "tvoc" on the LED display

The code also streams results over USB as a serial device, allowing readings to be viewed as they are collected in a serial monitor in real time.

It is recommended that the CCS811 be given a 48 hour initial burn in period and be allowed 20 minutes for the sensor to warm up to provide reliable readings thereafter.

The PCB layout can be opened in pcb-rnd, and gerbers for the shield have been provided in a zip file to allow ordering from the usual online PCB suppliers.


