/*
 * Implementation of the Brew Process
 * 
 * The Brew Process is owner of the Temp Sensor and the Heater Relay
 */
#ifndef BREWPROC_H_
#define BREWPROC_H_

#include "Arduino.h"


#include <DallasTemperature.h>
#include <NewRemoteTransmitter.h>

#define __DEBUG
#include "debug.h"

#include "brauwerkstatt.h"

// ==============================================
// Central data structures
// - receipe
// - proc_status
// ==============================================
#define MAX_RASTEN 10
#define MAX_HOPFENGABEN 10
#define HOPFENGABE_VWH 10000 // MAGIC Wert für Vorderwürzehopfung
#define HOPFENGABE_WHIRLPOOL 10001 // MAGIC Wert für Whirlpool-Hopfung

class BrewProcess {
public:

  BrewProcess(DallasTemperature* temp_sens, NewRemoteTransmitter* rf_sender);

  void init();

  void load_receipe();

  void start_mash_process();
  void start_second_wash_process(); // Nachguss
  void stop_process();
  void update_process();

  bool isRunning() { return _proc_stat.running; };
  char getPhaseChar() { return _proc_stat.phase_char; };
  bool needConfirmation() { return _proc_stat.need_confirmation; };
  void confirm() { _proc_stat.user_confirmed = true; };
  float getCurrentTemp() { return _temp_stat.current_temp; };
  float getTargetTemp() { return _proc_stat.target_temp; };
  char* getPhaseName() { return _proc_stat.phase_name; };
  char* getStepName() { return _proc_stat.step_name; };
  char* getPrompt() { return _proc_stat.user_prompt; };
  unsigned long phaseStart() { return _proc_stat.phase_start; };
  unsigned long procStart() { return _proc_stat.proc_start; };
  unsigned long phaseRest()
  {
    if (_proc_stat.current_phase == Phase::Rest && _proc_stat.current_step == Step::Hold)
    {
      unsigned long d = millis() - _proc_stat.rest_timer;
      if (d < _proc_stat.current_rest_duration)
      {
        return _proc_stat.current_rest_duration - d;
      }
      else
      {
        return 0;
      }
    }
    else
    {
      return 0;
    }
  };
  bool heaterOn() { return _heater_stat.on; };

  bool hasError() { return _proc_stat.has_error; };
  bool hasWarning() { return _proc_stat.has_warning; };
  char* getErrorMessage() { return _proc_stat.err_message; };
  char* getWarningMessage() { return _proc_stat.warn_message; };
  void resetError() { _proc_stat.has_error = false; };
  void resetWarning() { _proc_stat.has_warning = false; };

private:
  enum Phase { MashIn, Rest, MashOut, SecondWash, Boil };
  enum Step { Start, Heat, Hold, UserPrompt, Terminated};

  struct heater_stat_t {
    bool on = false;
    unsigned long last_on = 0; //millis of last on event
    unsigned long last_off = 0; // millis of last off event
  };

  struct proc_status_t {
    bool running = false;
    char phase_char = '-';
    char step_name[20];
    char phase_name[20];

    unsigned long proc_start = 0; // millis() zum Start des Brauvorgangs
    unsigned long phase_start; // millis() zum Start der aktuellen Phase
  
    Phase current_phase; // nummer der aktuellen Phase
    Step current_step;
    byte current_rast; // aktuelle rast
    unsigned long current_rest_duration; // duration for rest phases in minutes
  
    float target_temp; // Temperatur für die aktuelle Phase

    unsigned long rest_timer;
    
    bool need_confirmation = false; // for UI interaction: signal that user confirmation is required
    bool user_confirmed = false; // for UI interaction: user has confirmed a user prompt
    bool user_cancelled = false; // for UI interaction: user has cancelled brewing process
    
    char user_prompt[20];

    // error handling
    bool has_error = false;
    bool has_warning = false;
    char err_message[21];
    char warn_message[21];
  };

  struct receipe_t {
    bool loaded = false;
  
    byte einmaisch_temp;

    byte second_wash_temp;
  
    byte anz_rasten; // Anzahl Rasten, max. MAX_RESTS
    byte rast_temp[MAX_RASTEN]; // Rast-Temperaturen in Grad Celsius
    byte rast_dauer[MAX_RASTEN]; // Rast-Dauern in Minuten
  
    int wuerze_kochdauer;
    byte anz_hopfengaben; // Anzahl Hopfengaben, max. MAX_HOP_ADD
    int hopfengabe_zeit[MAX_HOPFENGABEN]; // Kochzeiten für Hopfen, wobei HOPFENGABE_VWH (10000) und HOPFENGABE_WHIRLPOOL (10001) gesondert behandelt werden
  };

  struct config_t {
    float heater_hysteresis = 1.0F; // if in temp hold mode, switch on heater when 1.0K below target temp
    float heater_throttle_diff = 2.0F; // throttle heater when approaching target temp by this amount
    float heater_off_diff = 0.5F; // turn off heater when arriving within this range of target temp
    float heater_cook_temp = 99.25F; // when reaching this temp, boiling timer is started
    unsigned int throttled_on_ms = 15000; // amount of time heater is "on" when in throttle mode
    unsigned int throttled_off_ms = 15000; // amount of time heater is "off" when in throttle mode
    unsigned int temp_read_interval = 5000; // read temperature every x ms
  };

  struct temp_sensor_t {
    bool currently_reading;
    unsigned long last_read_ms = 0;
    unsigned long last_conversion_trigger = 0;
    float current_temp; // Aktuelle Temperatur am Sensor
    DallasTemperature* temp_sensor;
    byte error_count = 0;
  };

  struct heater_stat_t _heater_stat;
  struct proc_status_t _proc_stat;
  struct temp_sensor_t _temp_stat;
  struct receipe_t _receipe;
  struct config_t _config;

  NewRemoteTransmitter* _rf_sender;

  void setWarning(const char* warnMsg) {
    _proc_stat.has_warning = true;
    strcpy_P(_proc_stat.warn_message, warnMsg);
  }

  void setError(const char* errMsg) {
    _proc_stat.has_error = true;
    strcpy_P(_proc_stat.err_message, errMsg);
    debug(F("********************"));
    debug(F("Error"));
    debug(_proc_stat.err_message);
    debug(F("********************"));
  };
  

  void read_temp_sensor();
  void setup_temp_sensor();
  
  void update_target_temp();
  void update_state_machine();
  void update_printable_name();
  void update_heater();

  void turn_on_heater();
  void turn_off_heater();
  void turn_on_heater_throttled();
  void phase_transition(Phase next_phase);
  void step_transition(Step next_step);
  void step_transition(Step next_step, const char* prompt);

  void handle_mash_in();
  void handle_rests();
  void handle_mash_out();
  void handle_second_wash();

  void start_rest_timer();
  bool is_rest_timer_over();
};

#endif /* BREWPROC_H_ */

