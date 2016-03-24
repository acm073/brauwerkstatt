

/*
 * Implementation of the Brew Process
 * 
 * The Brew Process is owner of the Temp Sensor and the Heater Relay
 * and also of the SD card.
 */
#ifndef BREWPROC_H_
#define BREWPROC_H_

#include "Arduino.h"


#include <DallasTemperature.h>
#include <NewRemoteTransmitter.h>
#include <Time.h>
#include <PetitFS.h>

#define __DEBUG
#include "debug.h"

#include "brauwerkstatt.h"

// ==============================================
// Central data structures
// - receipe
// - proc_status
// ==============================================
#define MAX_RESTS 5
#define MAX_HOP_ADDITIONS 6
#define HOP_ADD_FIRST_WORT 10000 // MAGIC value for first-wort hopping
#define HOP_ADD_WHIRLPOOL 10001 // MAGIC value for whirlpool hopping

class BrewProcess {
public:

  BrewProcess(DallasTemperature* temp_sens, NewRemoteTransmitter* rf_sender);

  void init();

  void load_receipe();

  void start_mash_process();
  void start_second_wash_process();
  void start_boil_process();
  void stop_process();
  void update_process();

  bool isRunning() { return _proc_stat.running; };
  char getPhaseChar() { return _proc_stat.phase_char; };
  bool needConfirmation() { return _proc_stat.need_confirmation; };
  void confirm() { _transient_proc_stat.user_confirmed = true; };
  float getCurrentTemp() { return _temp_stat.current_temp; };
  float getTargetTemp() { return _proc_stat.target_temp; };
  char* getDisplayName() { return _transient_proc_stat.display_name; };
  const char* getPrompt() { return _transient_proc_stat.user_prompt; };
  unsigned long phaseStart() { return _proc_stat.phase_start; };
  unsigned long procStart() { return _proc_stat.process_start; };
  unsigned long phaseRest()
  {
    if (_proc_stat.current_phase == Phase::Rest && _proc_stat.current_step == Step::Hold)
    {
      unsigned long d = now() - _proc_stat.rest_start;
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

  bool hasError() { return _transient_proc_stat.has_error; };
  bool hasWarning() { return _transient_proc_stat.has_warning; };
  char* getMessage() { return _transient_proc_stat.message; };
  void resetError() { _transient_proc_stat.has_error = false; };
  void resetWarning() { _transient_proc_stat.has_warning = false; };

private:
  enum Phase { MashIn, Rest, MashOut, SecondWash, Boil };
  enum Step { Start, Heat, Hold, UserPrompt, Terminated};
  enum ReceipeKey { Name, MashInTemp, Rests, RestTemp, RestDuration, SpargeTemp, BoilDuration, HopAdditions, HopBoilDuration };

  // ==========================================================
  // Heater status
  // ==========================================================
  struct heater_stat_t {
    bool on = false;
    unsigned long last_on = 0; //millis of last on event
    unsigned long last_off = 0; // millis of last off event
  };

  // ==========================================================
  // Process status, "permanent" part
  // this piece gets saved to EEPROM every minute
  // and contains all that is required to re-start the process
  // after e.g. power failure
  // ==========================================================
  struct proc_status_t {
    bool running = false;
    char phase_char = '-';

    // start of current process in seconds, as returned by now()
    unsigned long process_start;
    // start of current phase in seconds as returned by now()
    unsigned long phase_start;
    // start of current rest in seconds, as returned by now()
    // this value is only valid if current phase is Phase::Rest 
    unsigned long rest_start;

    Phase current_phase;
    Step current_step;
    byte current_rest;
    unsigned int current_rest_duration; // duration for rest phase in seconds

    bool need_confirmation = false; // for UI interaction: signal that user confirmation is required

    float target_temp; // target temperature for current phase

    // this value is also re-used as current system time after restore
    unsigned long eeprom_saved_timestamp = 0;

    // defined in brauwerkstatt.h
    unsigned long VERSION = PROC_STAT_VERSION;
  };

  // ==========================================================
  // Process status, "transient" part
  // this part does not get saved to EEPROM and is re-computed
  // from the permanent part after a restart.
  // Errors and warnings are reset after restart.
  // ==========================================================
  struct transient_proc_stat_t {
    char display_name[21];

    bool user_confirmed = false; // for UI interaction: user has confirmed a user prompt
    bool user_cancelled = false; // for UI interaction: user has cancelled brewing process
    
    const char user_prompt[4] = "Ok?";

    // error handling
    bool has_error = false;
    bool has_warning = false;
    char message[21];
  };

  // ==========================================================
  // The receipe is also saved to EEPROM
  // ==========================================================
  struct receipe_t {
    bool loaded = false;

    char name[9];
    byte mash_in_temp;

    byte second_wash_temp;
  
    byte num_rests; // number of rests
    byte rest_temp[MAX_RESTS]; // rest temperature in C
    byte rest_duration[MAX_RESTS]; // rest duration in min
  
    unsigned int wort_boil_duration; // in minutes
    byte num_hops_add; // number of hops additions
    unsigned int hops_boil_times[MAX_HOP_ADDITIONS]; // Kochzeiten für Hopfen, wobei HOPFENGABE_VWH (10000) und HOPFENGABE_WHIRLPOOL (10001) gesondert behandelt werden
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
  struct transient_proc_stat_t _transient_proc_stat;
  struct temp_sensor_t _temp_stat;
  struct receipe_t _receipe;
  struct config_t _config;

  NewRemoteTransmitter* _rf_sender;

  FATFS _sd_fs;
  
  void setWarning(const char* warnMsg) {
    _transient_proc_stat.has_warning = true;
    strcpy_P(_transient_proc_stat.message, warnMsg);
    debugnnl(F("WARN: ")); debug(_transient_proc_stat.message);
  }

  void setError(const char* errMsg) {
    _transient_proc_stat.has_error = true;
    strcpy_P(_transient_proc_stat.message, errMsg);
    debug(F("********************"));
    debug(F("Error"));
    debug(_transient_proc_stat.message);
    debug(F("********************"));
  };

  void recover_eeprom_state();
  bool parse_receipe_line(char* line);

  void read_temp_sensor();
  void setup_temp_sensor();
  
  void update_target_temp();
  void update_state_machine();
  void update_display_name();
  void update_heater();
  void update_eeprom(bool force);

  void turn_on_heater();
  void turn_off_heater();
  void turn_on_heater_throttled();
  void phase_transition(Phase next_phase);
  void step_transition(Step next_step);

  void handle_mash_in();
  void handle_rests();
  void handle_mash_out();
  void handle_second_wash();

  void start_rest_timer();
  bool is_rest_timer_over();

  void write_eeprom(byte* data, int size, int offset);
  void read_eeprom(byte* data, int size, int offset);
  void debug_state();
};

#endif /* BREWPROC_H_ */

