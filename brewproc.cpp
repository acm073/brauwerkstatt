#include <EEPROM.h>
#include "brewproc.h"

/**
 * Constructor
 * - assign pins
 * - init temp sensor
 * - init relay module
 * - force heater relay off
 */
BrewProcess::BrewProcess(DallasTemperature* temp_sens, NewRemoteTransmitter* rf_sender)
{  
  _temp_stat.temp_sensor = temp_sens;
  _rf_sender = rf_sender;
}

//====================================================================================================
// Public interface
//====================================================================================================
void BrewProcess::init()
{
  // Init Temp Sensors
  setup_temp_sensor();

  // Init Heater
  turn_off_heater();
}

/** 
 * Control routine of brew process
 * Just calls state updates in the right order
 */
void BrewProcess::update_process()
{
  // if we have an error, we do nothing until it has been reset.
  if (_proc_stat.has_error)
  {
    return;
  }
  
  // temp reading is done even if process is not running
  read_temp_sensor();

  if (_proc_stat.running)
  {
    update_target_temp();
    update_state_machine();
    update_heater();
    update_printable_name();
  }
  return;
}

void BrewProcess::start_second_wash_process()
{
  if (!_receipe.loaded)
  {
    debug(F("Rezept nicht geladen"));
  }
  else
  {
    if(!_proc_stat.running)
    {
      _proc_stat.current_phase = Phase::SecondWash;
      _proc_stat.current_step = Step::Start;
      
      _proc_stat.proc_start = millis();
      _proc_stat.phase_start = millis();
      _proc_stat.current_rast = -1;
      _proc_stat.running = true;
      _proc_stat.phase_char = 'N';
      update_process();

      debug(F("Nachguss initialisiert"));
    }
    else
    {
      debug(F("Process already running"));
    }
  }
}

void BrewProcess::start_mash_process()
{
  if (!_receipe.loaded)
  {
    debug(F("Rezept nicht geladen!"));
  }
  else
  {
    if (!_proc_stat.running)
    {
      _proc_stat.current_phase = Phase::MashIn;
      _proc_stat.current_step = Step::Start;
      
      _proc_stat.proc_start = millis();
      _proc_stat.phase_start = millis();
      _proc_stat.current_rast = -1;
      _proc_stat.running = true;
      _proc_stat.phase_char = 'M';
      update_process();

      debug(F("Maischen initialisiert"));
    }
    else
    {
      debug(F("Process already running!"));
    }
  }
}

void BrewProcess::load_receipe()
{
  _receipe.anz_rasten = 2;

  _receipe.einmaisch_temp = 30;
  _receipe.second_wash_temp = 43;

  _receipe.rast_temp[0] = 37;
  _receipe.rast_temp[1] = 41;
  //_receipe.rast_temp[2] = 72;
  //_receipe.rast_temp[3] = 78;

  _receipe.rast_dauer[0] = 1;
  _receipe.rast_dauer[1] = 1;
  //_receipe.rast_dauer[2] = 2;
  //_receipe.rast_dauer[3] = 2;

  _receipe.wuerze_kochdauer = 90;
  _receipe.anz_hopfengaben = 4;
  _receipe.hopfengabe_zeit[0] = HOPFENGABE_VWH;
  _receipe.hopfengabe_zeit[1] = 60;
  _receipe.hopfengabe_zeit[2] = 15;
  _receipe.hopfengabe_zeit[3] = HOPFENGABE_WHIRLPOOL;

  _receipe.loaded = true;
}

/*====================================================================================================
 * Implementation of state machine / process control
 * There are one to several methods for each supported process.
 * Supported Processes are:
 * - Mashing (done)
 * - Second wash heating (done)
 * - Boiling (planned)
 * - Cooling (planned)
 * ==================================================================================================== */
void BrewProcess::update_state_machine()
{
  switch(_proc_stat.current_phase)
  {
  case Phase::MashIn:
    handle_mash_in();
    break;
  case Phase::Rest:
    handle_rests();
    break;
  case Phase::MashOut:
    handle_mash_out();
    break;
  case Phase::SecondWash:
    handle_second_wash();
    break;
  case Phase::Boil:
    // TODO
    break;
  default:
    debugnnl(F("Ungueltige Phase "));
    debug(_proc_stat.current_phase);
  }
}

void BrewProcess::handle_second_wash()
{
  switch(_proc_stat.current_step)
  {
  case Step::Start:
    // nothing to do
    step_transition(Step::Heat);
    break;
  case Step::Heat:
    if(_temp_stat.current_temp >= _proc_stat.target_temp)
    {
      debug(F("Nachguss-Temperatur erreicht"));
      step_transition(Step::UserPrompt, PSTR("Beenden?"));
    }
    break;
  case Step::UserPrompt:
    if(_proc_stat.user_confirmed)
    {
      debug(F("User hat bestaetigt"));
      step_transition(Step::Terminated);
    }
    break;
  case Step::Terminated:
    _proc_stat.running = false;
    _proc_stat.phase_char = '-';
    break;
  default:
    debug(F("Invalid step"));
  }
}

void BrewProcess::handle_mash_out()
{
  switch(_proc_stat.current_step)
  {
  case Step::Start:
    // nothing to initialize, move to Heat immediately
    step_transition(Step::UserPrompt, PSTR("Beenden?"));
    break;
  case Step::UserPrompt:
    if(_proc_stat.user_confirmed)
    {
      debug(F("User hat bestaetigt"));
      step_transition(Step::Terminated);
    }
    break;
  case Step::Terminated:
    _proc_stat.running = false;
    _proc_stat.phase_char = '-';
    break;
  default:
    debugnnl(F("Ungueltiger Step fuer Abmaischen: "));
    debug(_proc_stat.current_step);
  }
}

void BrewProcess::handle_rests()
{
  /*
   * Rast-Phase:
   * - Wenn Temp_ist < Temp_soll[rast_num]: Step "Aufheizen"
   * - Wenn Temp erreicht: Zähler starten, Step "Halten"
   * - Wenn Rastdauer erreicht: nächste Rast oder Abmaischen
   * - Fall Temp_ist deutlich höher als Temp_soll wird nicht gehandelt
   */
  switch(_proc_stat.current_step)
  {
  case Step::Start:
    _proc_stat.current_rest_duration = _receipe.rast_dauer[_proc_stat.current_rast] * 60000;
    _proc_stat.phase_start = millis();
    step_transition(Step::Heat);
    break;
  case Step::Heat:
    if(_temp_stat.current_temp >= _proc_stat.target_temp)
    {
      debug(F("Rasttemperatur erreicht"));
      step_transition(Step::Hold);
      // expectation is to see current rest duration in display
      _proc_stat.phase_start = millis();
      start_rest_timer();
    }
    break;
  case Step::Hold:
    if(is_rest_timer_over())
    {
      debugnnl(F("Ende Rast "));
      debug(_proc_stat.current_rast);
      if(_proc_stat.current_rast + 1 < _receipe.anz_rasten)
      {
        // Last rest not reached, move to next rest
        ++_proc_stat.current_rast;
        step_transition(Step::Start);
      }
      else
      {
        // Last rest reached, move on to mash-out
        phase_transition(Phase::MashOut);
      }
    }
    break;
  default:
    debugnnl(F("Ungueltiger Step fuer Rasten: "));
    debug(_proc_stat.current_step);
  }
}

void BrewProcess::handle_mash_in()
{
  /*
   * Mash-In Phase:
   * - if current_temp < target_temp: step "Heat"
   * - if target_temp reached: "User Prompt"
   */
  switch(_proc_stat.current_step)
  {
  case Step::Start:
    // nothing to initialize, go to Step::Heat immediatly
    step_transition(Step::Heat);
    break;
  case Step::Heat:
    // check if target temp reached, then move on to UserPrompt
    if(_temp_stat.current_temp >= _proc_stat.target_temp)
    {
      // reached target temp
      debug(F("Einmaischtemperatur erreicht"));
      step_transition(Step::UserPrompt, PSTR("Weiter?"));
    }
    break;
  case Step::UserPrompt:
    // wait for confirmation, then transition to Phase::Rest
    if(_proc_stat.user_confirmed)
    {
      debug(F("User hat bestaetigt"));
      phase_transition(Phase::Rest);
      _proc_stat.current_rast = 0;
    }
    break;
  default:
    debugnnl(F("Ungueltiger Step "));
    debug(_proc_stat.current_step);
  }
}

void BrewProcess::phase_transition(Phase next_phase)
{
  _proc_stat.current_phase = next_phase;
  _proc_stat.phase_start = millis();
  _proc_stat.current_step = Step::Start;
  _proc_stat.need_confirmation = false;
  _proc_stat.user_confirmed = false;
}

void BrewProcess::step_transition(Step next_step)
{
  _proc_stat.current_step = next_step;
  if(next_step == Step::UserPrompt)
  {
    _proc_stat.need_confirmation = true;
    _proc_stat.user_confirmed = false;
  }
  else
  {
    _proc_stat.need_confirmation = false;
    _proc_stat.user_confirmed = false;    
  }
}

void BrewProcess::step_transition(Step next_step, const char* prompt)
{
  step_transition(next_step);
  strcpy(_proc_stat.user_prompt, prompt);
}

void BrewProcess::update_target_temp()
{
  switch(_proc_stat.current_phase)
  {
  case Phase::MashIn:
    _proc_stat.target_temp = _receipe.einmaisch_temp;
    break;
  case Phase::Rest:
    _proc_stat.target_temp = _receipe.rast_temp[_proc_stat.current_rast];
    break;
  case Phase::MashOut:
    _proc_stat.target_temp = -1;
    break;
  case Phase::Boil:
    _proc_stat.target_temp = _config.heater_cook_temp;
    break;
  default:
    _proc_stat.target_temp = -1;
  }
} 

void BrewProcess::update_printable_name()
{
  switch(_proc_stat.current_phase)
  {
  case Phase::MashIn:
    strcpy(_proc_stat.phase_name, "Einmaischen");
    break;
  case Phase::Rest:
    sprintf(_proc_stat.phase_name, "Rast #%d", _proc_stat.current_rast + 1);
    break;
  case Phase::MashOut:
    strcpy(_proc_stat.phase_name, "Abmaischen");
    break;
  case Phase::Boil:
    strcpy(_proc_stat.phase_name, "Kochen");
    break;
  case Phase::SecondWash:
    strcpy(_proc_stat.phase_name, "Nachguss");
  default:
   strcpy(_proc_stat.phase_name, "undef");
   break;
  }
  
  switch(_proc_stat.current_step)
  {
  case Step::Heat:
    strcpy(_proc_stat.step_name, "Heizen");
    break;
  case Step::Hold:
    strcpy(_proc_stat.step_name, "Halten");
    break;
  default:
    strcpy(_proc_stat.step_name, "");
    break;
  }
}


/*
 * This function implements the heater control algorithm. For now it is a simple 2-point controller.
 * Temperature values below are default values, actual values are stored in config structure.
 * 
 * temp_diff = t_target - t_current
 * if STEP_HEAT
 *   if temp_diff >= 2K: heater on
 *   if temp_diff between 2K and 0.5K: heater throttled
 *   if temp_diff < 0.5K: heater off
 * if STEP_HOLD
 *   if temp_diff < 0.5K: heater off
 *   if temp_diff > 1K: heater throttled on
 */
void BrewProcess::update_heater()
{
  float temp_diff = _proc_stat.target_temp - _temp_stat.current_temp;
  switch(_proc_stat.current_step)
  {
  case Step::Heat:
    // still away from target -> heater on
    if(temp_diff >= _config.heater_throttle_diff)
    {
      turn_on_heater();
    }
    // approaching in throttled mode
    else if (temp_diff < _config.heater_throttle_diff && temp_diff > _config.heater_off_diff)
    {
      turn_on_heater_throttled();
    }
    // close enough or above --> heater off
    else
    {
      turn_off_heater();
    }
    break;
  case Step::Hold:
  case Step::UserPrompt:
    if(temp_diff > _config.heater_hysteresis)
    {
      turn_on_heater_throttled();
    }
    else if (temp_diff <= _config.heater_off_diff)
    {
      turn_off_heater();
    }
    break;
  default:
    _heater_stat.on = false;
    turn_off_heater();
  }
}

// ====================================================
// heater management
// ====================================================
void BrewProcess::turn_on_heater_throttled()
{
  if(_heater_stat.on && millis() - _heater_stat.last_on > _config.throttled_on_ms)
  {
    turn_off_heater();
  }
  if(!_heater_stat.on && millis() - _heater_stat.last_off > _config.throttled_off_ms)
  {
    turn_on_heater();
  }
}

void BrewProcess::turn_off_heater()
{
  if (_heater_stat.on)
  {
    _heater_stat.on = false;
    _heater_stat.last_off = millis();
  }
  _rf_sender->sendUnit(RC_OUTLET_HEATER, false);
}

void BrewProcess::turn_on_heater()
{
  if (!_heater_stat.on)
  {
    _heater_stat.on = true;
    _heater_stat.last_on = millis();
  }
  _rf_sender->sendUnit(RC_OUTLET_HEATER, true);
}

// ====================================================
// temperature sensor management
// ====================================================
void BrewProcess::read_temp_sensor ()
{
  if(_temp_stat.error_count > 3 && _temp_stat.error_count < 5)
  {
    setup_temp_sensor();
  }
  else if (_temp_stat.error_count > 5)
  {
    setError(PSTR("Temp Sensor Error"));
  }
  if (millis() - _temp_stat.last_read_ms > _config.temp_read_interval)
  {
    if (_temp_stat.currently_reading)
    {
      // we are in a conversion cycle
      if (millis() - _temp_stat.last_conversion_trigger > TEMP_SENSOR_CONVERSION_TIME)
      {
        // we're done
        DeviceAddress tempDeviceAddress;
        if(_temp_stat.temp_sensor->getAddress(tempDeviceAddress, 0))
        {
          float t = _temp_stat.temp_sensor->getTempC(tempDeviceAddress);
          if(t == 85.0F || t == -127.0F)
          {
            debug(F("Got bogus reading"));
            _temp_stat.error_count++;
          }
          else
          {
            _temp_stat.current_temp = t;
            _temp_stat.error_count = 0;
          }
        }
        else
        {
          _temp_stat.error_count++;
        }
        _temp_stat.currently_reading = false;
        _temp_stat.last_read_ms = millis();
      }
    }
    else
    {
      _temp_stat.temp_sensor->requestTemperatures();
      _temp_stat.currently_reading = true;
      _temp_stat.last_conversion_trigger = millis();
    }    
  }
}

void BrewProcess::setup_temp_sensor()
{
  _temp_stat.temp_sensor->setWaitForConversion(false);
  _temp_stat.temp_sensor->begin();

  DeviceAddress tempDeviceAddress;
  if(_temp_stat.temp_sensor->getAddress(tempDeviceAddress, 0))
  {
    _temp_stat.temp_sensor->setResolution(tempDeviceAddress, 11);
  }
  else
  {
    debug(F("No temperature sensor found"));
  }
}

// ====================================================
// timer for timing mashing rests
// ====================================================

void BrewProcess::start_rest_timer()
{
  _proc_stat.rest_timer = millis();
}

bool BrewProcess::is_rest_timer_over()
{
  return millis() - _proc_stat.rest_timer > _proc_stat.current_rest_duration;
}

