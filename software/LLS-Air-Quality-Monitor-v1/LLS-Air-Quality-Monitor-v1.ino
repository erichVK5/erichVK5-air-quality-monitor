/*
 *
 *   Copyright (C) 2021 Erich S. Heinzle
 *   Website https://github.com/Lobethal-Lutheran-School/
 *   
 *   Lobethal Lutheran School - "Historically Grounded, Future Minded"
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License at <http://www.gnu.org/licenses/> 
 *   for more details.
 *
 */
 
#include "Arduino.h"
#include "TM1637Display.h"
#include "Adafruit_Sensor.h"
#include "Adafruit_AM2320.h"
#include "Adafruit_CCS811.h"
#include "RTClib.h"
#include "Wire.h"
#include "SD.h"

// TM1637 Module connection pins (Digital Pins)
#define CLK 2
#define DIO 3
// Pin 10 CS required by SD card code
#define CHIP_SELECT 10

// Time (milliseconds) between sensor readings
// need 8*DISPLAY_PAUSE < MS_BETWEEN_SAMPLES 
#define DISPLAY_PAUSE 1200 

// NB caching more than 18 sample sets tends to exceed
// dynamic RAM needs of code running on atmega328
// so writing batches of 18 samples to SD is about right
//
#define MS_BETWEEN_SAMPLES 10000
// samples per minute = 60000/MS_BETWEEN_SAMPLES 
#define SAMPLES_PER_WRITE 18
// minutes between sample writes to SD = SAMPLES_PER_WRITE/samples per minute 

// Struct used for storing time stamp for a set of readings
typedef struct time_stamp_t {
  uint8_t Year;
  uint8_t Month;
  uint8_t Day;
  uint8_t Hour;
  uint8_t Min;
  uint8_t Sec;
};  

// Struct used for caching sets of readings between SD card writes
typedef struct ring_buffer_t {
  time_stamp_t time_stamp[SAMPLES_PER_WRITE];
  uint8_t temp[SAMPLES_PER_WRITE];     // essentially guaranteed to be less than 256
  uint8_t humidity[SAMPLES_PER_WRITE]; // less than 101 by definition
  uint16_t co2[SAMPLES_PER_WRITE];     // usually exceeds 256
  uint16_t tvoc[SAMPLES_PER_WRITE];    // may exceed 256
  uint8_t index;
};

// four digit definition for "LLS"
const uint8_t SEG_LLS[] = {
  SEG_D | SEG_E | SEG_F,                           // L
  SEG_D | SEG_E | SEG_F,                           // L
  SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,           // S
  0
  };

// four digit definition for "CO2"
const uint8_t SEG_CO2[] = {
  SEG_A | SEG_D | SEG_E | SEG_F,                   // C
  SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,   // O
  0,
  SEG_A | SEG_B | SEG_D | SEG_E | SEG_G            // 2
  };

// four digit definition for "t" - temperature
const uint8_t SEG_T[] = {
  SEG_D | SEG_E | SEG_F | SEG_G,                   // t
  0,
  0,
  0
  };

// four digit definition for "h" - humidity
const uint8_t SEG_H[] = {
  SEG_C | SEG_E | SEG_E | SEG_F | SEG_G,           // h
  0,
  0,
  0
  };

// four digit definition for "TVOC" - total VOCs
const uint8_t SEG_TVOC[] = {
  SEG_D | SEG_E | SEG_F | SEG_G,                   // t
  SEG_C | SEG_D | SEG_E,                           // v
  SEG_C | SEG_D | SEG_E | SEG_G,                   // o
  SEG_D | SEG_E | SEG_G,                           // c
  };

// four digit definition for "SD" - writes to SD card
const uint8_t SEG_SD[] = {
  SEG_A | SEG_C | SEG_D | SEG_F | SEG_G,           // S
  SEG_B | SEG_C | SEG_D | SEG_E | SEG_G,           // d
  0,
  0
  };

TM1637Display display(CLK, DIO);
Adafruit_AM2320 am2320 = Adafruit_AM2320();
Adafruit_CCS811 ccs811;
RTC_PCF8523 RTC;
File logfile;

// mathematics using unsigned long can cope with rollover
const unsigned long samplingPeriodMillis = MS_BETWEEN_SAMPLES;

void setup()
{
    pinMode(CHIP_SELECT, OUTPUT);  
    // open serial port to stream data
    Serial.begin(57600);

    // start sensors
    am2320.begin();
    ccs811.begin();

    // wait for CCS811 startup
    while(!ccs811.available());

    // check if SD card is present and working
    if (!SD.begin(CHIP_SELECT)) {
      Serial.println("SD init failure/not present");
      return;
    } else {
      Serial.println("# SD initialised");
    }

    // create new, incrementally numbered filename
    char filename[] = "LLSAQM00.CSV";
    for (uint8_t i = 0; i < 100; i++) {
      filename[6] = i/10 + '0';
      filename[7] = i%10 + '0';
      // open the new file if it does not exist, then leave loop
      if (!SD.exists(filename)) {
        logfile = SD.open(filename, FILE_WRITE);
        Serial.print("# Created logfile: ");
        Serial.println(filename);
        logfile.println("# Logger unit 1");
        logfile.println("Date,Time,Temp,Humidity,CO2,TVOC");
        break;
      }
    }

    // check that logfile was successfully created
    if (!logfile) {
      Serial.println("Failed to create logfile");
      return;
    }

    Wire.begin();
    // check if RTC is working
    if (!RTC.begin()) {
      logfile.println("RTC failed");
      Serial.println("RTC failed");
    }
    Serial.println("# Completed setup");
    Serial.println("Date,Time,Temp,Humidity,CO2,TVOC");

}

// Routine to display latest set of readings from cached values on LED display
void display_readings(ring_buffer_t *ring_buffer) {
  display.setSegments(SEG_T);
  delay(DISPLAY_PAUSE);
  display.showNumberDec(ring_buffer->temp[ring_buffer->index], false);
  delay(DISPLAY_PAUSE);
  display.setSegments(SEG_H);
  delay(DISPLAY_PAUSE);
  display.showNumberDec(ring_buffer->humidity[ring_buffer->index], false);
  delay(DISPLAY_PAUSE);
  display.setSegments(SEG_CO2);
  delay(DISPLAY_PAUSE);
  display.showNumberDec(ring_buffer->co2[ring_buffer->index], false);
  delay(DISPLAY_PAUSE);
  display.setSegments(SEG_TVOC);
  delay(DISPLAY_PAUSE);
  display.showNumberDec(ring_buffer->tvoc[ring_buffer->index], false);
  delay(DISPLAY_PAUSE);
}

// Routine to write cached readings to SD card
void write_ring_buffer_if_full(ring_buffer_t *ring_buffer) {
  int i;
  if(ring_buffer->index == SAMPLES_PER_WRITE) {
    display.setSegments(SEG_SD);
    for(i = 0; i < SAMPLES_PER_WRITE; i++) {
      logfile.print(ring_buffer->time_stamp[i].Day);
      logfile.print('/');
      logfile.print(ring_buffer->time_stamp[i].Month);
      logfile.print('/');
      logfile.print(ring_buffer->time_stamp[i].Year);
      logfile.print(',');
      if (ring_buffer->time_stamp[ring_buffer->index].Hour < 10) {
        logfile.print('0');
      }
      logfile.print(ring_buffer->time_stamp[i].Hour);
      logfile.print(':');
      if (ring_buffer->time_stamp[ring_buffer->index].Min < 10) {
        logfile.print('0');
      }
      logfile.print(ring_buffer->time_stamp[i].Min);
      logfile.print(':');
      if (ring_buffer->time_stamp[ring_buffer->index].Sec < 10) {
        logfile.print('0');
      }
      logfile.print(ring_buffer->time_stamp[i].Sec);
      logfile.print(',');
      logfile.print(ring_buffer->temp[i]);
      logfile.print(',');
      logfile.print(ring_buffer->humidity[i]);
      logfile.print(',');
      logfile.print(ring_buffer->co2[i]);
      logfile.print(',');
      logfile.println(ring_buffer->tvoc[i]);
      logfile.flush();
    }
    // reset index to zero to start filling cache from beginning again
    ring_buffer->index = 0;  
    Serial.println("# Samples->SD");
  }
}

// Routine to stream latest set of readings to serial output
void current_readings_to_serial(ring_buffer_t *ring_buffer) {
      Serial.print(ring_buffer->time_stamp[ring_buffer->index].Day);
      Serial.print('/');      
      Serial.print(ring_buffer->time_stamp[ring_buffer->index].Month);
      Serial.print('/');
      Serial.print(ring_buffer->time_stamp[ring_buffer->index].Year);
      Serial.print(',');
      if (ring_buffer->time_stamp[ring_buffer->index].Hour < 10) {
        Serial.print('0');
      }
      Serial.print(ring_buffer->time_stamp[ring_buffer->index].Hour);
      Serial.print(':');
      if (ring_buffer->time_stamp[ring_buffer->index].Min < 10) {
        Serial.print('0');
      }
      Serial.print(ring_buffer->time_stamp[ring_buffer->index].Min);
      Serial.print(':');
      if (ring_buffer->time_stamp[ring_buffer->index].Sec < 10) {
        Serial.print('0');
      }
      Serial.print(ring_buffer->time_stamp[ring_buffer->index].Sec);
      Serial.print(',');
      Serial.print(ring_buffer->temp[ring_buffer->index]);
      Serial.print(',');
      Serial.print(ring_buffer->humidity[ring_buffer->index]);
      Serial.print(',');
      Serial.print(ring_buffer->co2[ring_buffer->index]);
      Serial.print(',');
      Serial.println(ring_buffer->tvoc[ring_buffer->index]);
}

void loop()
{

  unsigned long startMillis;
  unsigned long currentMillis;
  DateTime dt;
  ring_buffer_t ring_buffer;

  // initialise ring buffer
  ring_buffer.index = 0;

  // turn display on
  display.setBrightness(7, true);
  display.setSegments(SEG_LLS);
  delay(DISPLAY_PAUSE);

  startMillis = millis();

  while (1) {

    dt = RTC.now();
    ring_buffer.time_stamp[ring_buffer.index].Year = dt.year()-208;
    ring_buffer.time_stamp[ring_buffer.index].Month = dt.month();
    ring_buffer.time_stamp[ring_buffer.index].Day = dt.day();
    ring_buffer.time_stamp[ring_buffer.index].Hour = dt.hour();
    ring_buffer.time_stamp[ring_buffer.index].Min = dt.minute();
    ring_buffer.time_stamp[ring_buffer.index].Sec = dt.second();

    ring_buffer.temp[ring_buffer.index] = (int)am2320.readTemperature();
    ring_buffer.humidity[ring_buffer.index] = (int)am2320.readHumidity();

    if(ccs811.available()){
      if(!ccs811.readData()){
        ring_buffer.co2[ring_buffer.index] = ccs811.geteCO2();
        ring_buffer.tvoc[ring_buffer.index] = ccs811.getTVOC();
      }
    } else {
        Serial.println("E: CO2/TVOC unavailable");
        logfile.println("E: CO2/TVOC unavailable");
        ring_buffer.co2[ring_buffer.index] = 0;
        ring_buffer.tvoc[ring_buffer.index] = 0;
    }

    display_readings(&ring_buffer);
    current_readings_to_serial(&ring_buffer);

    ring_buffer.index++;
    write_ring_buffer_if_full(&ring_buffer);

    // wait for next round of sampling
    currentMillis = millis();
    while (currentMillis - startMillis < samplingPeriodMillis) {
      delay(10);
      currentMillis = millis();
    }
    startMillis = currentMillis;
  }
}
