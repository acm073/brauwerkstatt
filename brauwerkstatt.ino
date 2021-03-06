/*
 * Hardware
 *
 * uC: Arduino nano (CN Clone)
 *
 * Inputs:
 * - Temp Sensor 1 (Maische/Nachguss): DS18B20 an Pin 3
 * - Temp Sensor 2 (Nachguss optional): DS18B20 an Pin 3
 * - User Input: KY-040 Drehgeber (CLK an Pin 7, DT an Pin 8, SW an Pin 9)
 *
 * Outputs:
 * - LCD: 20x4 mit I2C an A4 und A5
 * - Relais Heizung: Pin 8
 *
 * Peripherie:
 * - ESP8266 WLAN Modul an RX (Pin 0) und TX (Pin 1)
 * - SD Card Reader (CS: Pin 10, MOSI: Pin 11, MISO: Pin 12, SCK: Pin 13)
 * - RTC an I2C Bus (A4, A5)
 *
 * Pin Layout:
 *  0 (RX) - Debug
 *  1 (TX) - Debug
 *  2 (GPIO) - Encoder A
 *  3 (GPIO) - Encoder Switch
 *  4 (GPIO) - Encoder B
 *  5 (GPIO) - 1Wire Bus fuer Temp Sensors
 *  6 (GPIO) - NC
 *  7 (GPIO) - NC
 *  8 (GPIO) - RF Transmitter to switch on heater
 *  9 (GPIO) - NC
 * 10 (GPIO) - SD Card Reader CS
 * 11 (GPIO) - SD Card Reader MOSI
 * 12 (GPIO) - SD Card Reader MISO
 * 13 (GPIO) - SD Card Reader SCK
 * A0 - ESP8266 reset
 * A1 - NC
 * A2 - NC
 * A3 - NC
 * A4 - LCD SDA (- RTC SDA)
 * A5 - LCD SCL (- RTC SCL)
 * A6 - NC
 * A7 - NC
 *
 * ------
 *
 * Funktionalitaet
 *
 * Startmenue:
 * - Brauen
 * - Config:
 *
 * Brauen:
 * - Rezept eingeben
 * - Rezept von SD laden
 * - Rezept von Server laden
 * - Start
 *
 * Config
 * - Netzwerk suchen
 * - Wlan Key eingeben
 * - Temperaturgradienten konfigurieren
 *
 * Generelle Features
 * - Ausgabe Temp
 * - Ausgabe Relais-Status
 * - Anzeige aktueller Prozessschritt (Schritt, Laufzeit, Restdauer)
 * - Logging Prozessdaten
 *
 * Brauprozess
 * - Prompt: Hauptguss einfüllen
 * - Prompt: Rührwerk starten
 * - Aufheizen Hauptguss
 * - Prompt: Einmaischen
 * - Pro Rast
 *   - Temperatur anfahren
 *   - Temperatur halten
 * - Prompt: Abmaischen
 * - Prompt: Nachgusswasser einfüllen
 * - Aufheizen Nachguss
 * - Temperatur halten Nachguss
 *
 */

// header file contains:
// - feature toggles
// - pin definitions
#define __DEBUG

#include "brauwerkstatt.h"

#include <TimerOne.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LiquidCrystal_I2C.h>
#include <NewRemoteTransmitter.h>
#include <EEPROM.h>

#include "brewproc.h"
#include "brewui.h"
#include "debug.h"

#ifdef __WIFI
ESP8266wifi wifi(Serial, Serial, WIFI_RESET_PIN);
#endif

// init all peripherals
OneWire one_wire(TEMP_SENSOR_PIN);
DallasTemperature temp_sensor(&one_wire);
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_LINES);
NewRemoteTransmitter rf_sender(RF_TRANSMITTER_ID, RF_TRANSMITTER_PIN, RF_TRANSMITTER_PULSE_LENGTH_US, RF_TRANSMITTER_REPEATS);

// init main classes
BrewProcess brewProc(&temp_sensor, &rf_sender);
BrewUi brewUi(&brewProc, &lcd, ENC_A_PIN, ENC_B_PIN, ENC_SW_PIN);


void setup() {
  // Init Timer Interrupt (for encoder)
  Timer1.initialize(INPUT_ISR_DELAY);
  Timer1.attachInterrupt(&timer_isr);

  // init Serial
  while (!Serial); // wait for Serial to become available
  Serial.begin(9600);

  brewProc.init();
  brewUi.init();

  debug(F("Debug logging on HW Serial, ver 0002"));
}

int count = 0;
unsigned long start = 0;
unsigned long ui_duration = 0;
unsigned long proc_duration = 0;
void loop()
{
  start = micros();
  brewUi.update_ui();
  ui_duration += (micros() - start);

  start = micros();  
  brewProc.update_process();
  proc_duration += (micros() - start);

  ++count;

  if (count == 200)
  {
    // debugnnl(F("Avg ui update ")); debugnnl(ui_duration / 200); debug(F("us"));
    // debugnnl(F("Avg proc update ")); debugnnl(proc_duration / 200); debug(F("us"));
    ui_duration = 0;
    proc_duration = 0;
    count = 0;
  }
}

void timer_isr()
{
  brewUi.encoder_isr();
}

