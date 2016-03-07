/*
 * brauwerkstatt.h
 *
 *  Created on: 18.02.2016
 *      Author: amessne
 */

#ifndef BRAUWERKSTATT_H_
#define BRAUWERKSTATT_H_

// Feature Toggles
#undef __SD   // SD Card
#undef __WIFI // Wifi
// define if input happens via serial line
// reads a char and emulates encoder like this:
// implementation for this behaviour is in the Encoder class
#define INPUT_SERIAL 1


#ifdef __SD
#include <SD.h>
#endif

#ifdef __WIFI
#include <ESP8266wifi.h>
#endif

// ==============================================
// Peripherals configuration
// ==============================================

// 1. LCD
#define LCD_ADDRESS 0x27
#define LCD_LINES 4
#define LCD_COLS 20

// 2. Temperature Sensor
#define TEMP_SENSOR_PIN 5
#define TEMP_SENSOR_RESOLUTION 12
#define TEMP_SENSOR_CONVERSION_TIME 750

// 3. RF Transmitter (to switch heater)
#define RF_TRANSMITTER_PIN 8
#define RF_TRANSMITTER_ID 123
#define RF_TRANSMITTER_PULSE_LENGTH_US 260
#define RF_TRANSMITTER_REPEATS 3 // actual repeats are 2^REPEATS
#define RC_OUTLET_HEATER 1 // Remote Control Outlet ID of heater outlet

// 4. SD Card
#define SD_CS_PIN 10

// 5. Encoder
#define ENC_A_PIN 2
#define ENC_B_PIN 4
#define ENC_SW_PIN 3

// 6. WIFI
#define WIFI_BAUD_RATE 115200
#define WIFI_RESET_PIN A0

// set to 5000us for serial
// set to 1000us for real encoder
#ifdef INPUT_SERIAL
  #define INPUT_ISR_DELAY 5000
#else
  #define INPUT_ISR_DELAY 1000
#endif

#endif /* BRAUWERKSTATT_H_ */
