#include "brewproc.h"
#include <Time.h>
#include <EEPROM.h>

/**
 * Constructor
 */
BrewProcess::BrewProcess(DallasTemperature* temp_sens, NewRemoteTransmitter* rf_sender)
{  
  _temp_stat.temp_sensor = temp_sens;
  _rf_sender = rf_sender;
}

//====================================================================================================
// Public interface
//====================================================================================================
/**
 * Init is called from setup() in main sketch.
 */
void BrewProcess::init()
{
    // Init SD Card
  if (pf_mount(&_sd_fs) != FR_OK)
  {
    setError(PSTR("SD-Karten-Fehler"));
  }

  // state recovery from eeprom
  recover_eeprom_state();

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
  if (_transient_proc_stat.has_error)
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
    update_display_name();
    update_eeprom(false);
  }
  return;
}

void BrewProcess::stop_process()
{
  _proc_stat.running = false;
  turn_off_heater();
  update_eeprom(true);
}

void BrewProcess::start_boil_process()
{
  //TODO
}

void BrewProcess::start_second_wash_process()
{
  if (!_receipe.loaded)
  {
    setWarning(PSTR("Kein Rezept"));
  }
  else
  {
    if(!_proc_stat.running)
    {
      _proc_stat.current_phase = Phase::SecondWash;
      _proc_stat.current_step = Step::Start;
      
      _proc_stat.process_start = now();
      _proc_stat.phase_start = now();
      _proc_stat.current_rest = -1;
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
      
      _proc_stat.process_start = now();
      _proc_stat.phase_start = now();
      _proc_stat.current_rest = -1;
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
  if(pf_open("REZEPT.TXT"))
  {
    setError(PSTR("REZEPT.TXT fehlt"));
    return;
  }

  char buf[32];
  char line[41];
  int b = 0;
  while(true)
  {
    unsigned int cnt;
    pf_read(buf, sizeof(buf), &cnt);
    if (cnt == 0)
    {
      break;
    }
    int i = 0;
    for (; i < cnt; i++)
    {
      if(buf[i] == '\r' || buf[i] == '\n')
      {
        // line is finished, parse it
        line[b] = '\0';
        if (strlen(line) > 0)
        {
          if(!parse_receipe_line(line))
          {
            setError("Parse-Fehler");
            return;
          }
        }
        b = 0;
      }
      else
      {
        if (b >= sizeof(line))
        {
          setError(PSTR("Zeile zu lange"));
          return;
        }
        line[b] = buf[i];
        b++;
      }
    }
  }
  /*
  _receipe.num_rests = 2;

  _receipe.mash_in_temp = 30;
  _receipe.second_wash_temp = 41;

  _receipe.rest_temp[0] = 37;
  _receipe.rest_temp[1] = 41;
  //_receipe.rast_temp[2] = 72;
  //_receipe.rast_temp[3] = 78;

  _receipe.rest_duration[0] = 1;
  _receipe.rest_duration[1] = 1;
  //_receipe.rast_dauer[2] = 2;
  //_receipe.rast_dauer[3] = 2;

  _receipe.wort_boil_duration = 90;
  _receipe.num_hops_add = 4;
  _receipe.hops_boil_times[0] = HOP_ADD_FIRST_WORT;
  _receipe.hops_boil_times[1] = 60;
  _receipe.hops_boil_times[2] = 15;
  _receipe.hops_boil_times[3] = HOP_ADD_WHIRLPOOL;
  */
  _receipe.loaded = true;
}

bool BrewProcess::parse_receipe_line(char* line)
{
  debugnnl(F("rcpt_line: ")); debug(line);
  if (line[0] == '#') return true; // skip comments
  char key[12];
  char val[9];
  byte idx = 0; // rests and hops additions
  bool after_eq = false;
  bool val_is_number = true;
  int num_val;
  key[0] = '\0';
  val[0] = '\0';
  for (int k = 0; k < strlen(line); k++)
  {
    if (isspace(line[k])) continue;
    if (isdigit(line[k]) && !after_eq)
    {
      idx = line[k] - '0';
    }
    if (line[k] == '=')
    {
      after_eq = true;
      continue;
    }
    if (after_eq)
    {
      int len = strlen(val);
      if (len >= sizeof(val))
      {
        debug(F("Value too long"));
        return false;
      }
      val[len] = line[k];
      val[len + 1] = '\0';
      if (!isdigit(line[k])) val_is_number = false;
    }
    else
    {
      int len = strlen(key);
      if (len >= sizeof(key))
      {
        debug(F("Key too long"));
        return false;
      }
      key[len] = line[k];
      key[len + 1] = '\0';
    }
  }
  if (!after_eq)
  {
    debug(F("= missing"));
    return false;
  }
  if (val_is_number)
  {
    num_val = atoi(val);
  }

  // enum ReceipeKey { Name, MashInTemp, Rests, RestTemp, RestDuration, SpargeTemp, BoilDuration, HopAdditions, HopBoilDuration };
  ReceipeKey rcp_key;
  if(strcmp_P(key, PSTR("name")) == 0) rcp_key = ReceipeKey::Name;
  else if (strcmp_P(key, PSTR("einmaisch_t")) == 0) rcp_key = ReceipeKey::MashInTemp;
  else if (strcmp_P(key, PSTR("rasten")) == 0) rcp_key = ReceipeKey::Rests;
  else if (strcmp_P(key, PSTR("nachguss_t")) == 0) rcp_key = ReceipeKey::SpargeTemp;
  else if (strcmp_P(key, PSTR("koch_d")) == 0) rcp_key = ReceipeKey::BoilDuration;
  else if (strcmp_P(key, PSTR("hopfengaben")) == 0) rcp_key = ReceipeKey::HopAdditions;
  else
  {
    // parse the dynamic entries
    if (idx == 0) return false;
    if (strncmp_P(key, PSTR("rast"), 4) == 0 &&
      strlen(key) == 7 &&
      key[4] - '0' == idx &&
      key[5] == '_')
    {
      switch(key[6])
      {
      case 't':
        rcp_key = ReceipeKey::RestTemp;
        break;
      case 'd':
        rcp_key = ReceipeKey::RestDuration;
        break;
      default:
        return false;
      }
    }
    else if (strncmp_P(key, PSTR("hopfengabe"), 10) &&
      strlen(key) == 11 &&
      key[10] - '0' == idx)
    {
      rcp_key == ReceipeKey::HopBoilDuration;
    }
    else
    {
      return false;
    }
  }
  if (rcp_key != ReceipeKey:: HopBoilDuration && rcp_key != ReceipeKey::Name && !val_is_number)
  {
    return false;
  }
  switch (rcp_key)
  {
  case ReceipeKey::Name:
    strcpy(_receipe.name, val);
    break;
  case ReceipeKey::MashInTemp:
    _receipe.mash_in_temp = num_val;
    break;
  case ReceipeKey::Rests:
    _receipe.num_rests = num_val;
    break;
  case ReceipeKey::RestTemp:
    if(idx < 1 || idx > _receipe.num_rests) return false;
    _receipe.rest_temp[idx - 1] = num_val;
    break;
  case ReceipeKey::RestDuration:
    if(idx < 1 || idx > _receipe.num_rests) return false;
    _receipe.rest_duration[idx - 1] = num_val;
    break;
  case ReceipeKey::SpargeTemp:
    _receipe.second_wash_temp = num_val;
    break;
  case ReceipeKey::BoilDuration:
    _receipe.wort_boil_duration = num_val;
    break;
  case ReceipeKey::HopAdditions:
    _receipe.num_hops_add = num_val;
    break;
  case ReceipeKey::HopBoilDuration:
    if(idx < 1 || idx > _receipe.num_hops_add) return false;
    if (val_is_number)
    {
      _receipe.hops_boil_times[idx - 1] = num_val;
    }
    else if(strcmp_P(val, PSTR("VW")) == 0)
    {
      _receipe.hops_boil_times[idx - 1] = HOP_ADD_FIRST_WORT;
    }
    else if(strcmp_P(val, PSTR("WP")) == 0)
    {
      _receipe.hops_boil_times[idx - 1] = HOP_ADD_WHIRLPOOL;
    }
    else
    {
      return false;
    }
    break;
  }
  
  return true;
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
      step_transition(Step::UserPrompt);
    }
    break;
  case Step::UserPrompt:
    if(_transient_proc_stat.user_confirmed)
    {
      debug(F("User hat bestaetigt"));
      step_transition(Step::Terminated);
    }
    break;
  case Step::Terminated:
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
    step_transition(Step::UserPrompt);
    break;
  case Step::UserPrompt:
    if(_transient_proc_stat.user_confirmed)
    {
      debug(F("User hat bestaetigt"));
      step_transition(Step::Terminated);
    }
    break;
  case Step::Terminated:
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
    _proc_stat.current_rest_duration = _receipe.rest_duration[_proc_stat.current_rest] * 60;
    _proc_stat.phase_start = now();
    step_transition(Step::Heat);
    break;
  case Step::Heat:
    if(_temp_stat.current_temp >= _proc_stat.target_temp)
    {
      debug(F("Rasttemperatur erreicht"));
      step_transition(Step::Hold);
      // expectation is to see current rest duration in display
      _proc_stat.phase_start = now();
      start_rest_timer();
    }
    break;
  case Step::Hold:
    if(is_rest_timer_over())
    {
      debugnnl(F("Ende Rast "));
      debug(_proc_stat.current_rest);
      if(_proc_stat.current_rest + 1 < _receipe.num_rests)
      {
        // Last rest not reached, move to next rest
        ++_proc_stat.current_rest;
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
      step_transition(Step::UserPrompt);
    }
    break;
  case Step::UserPrompt:
    // wait for confirmation, then transition to Phase::Rest
    if(_transient_proc_stat.user_confirmed)
    {
      debug(F("User hat bestaetigt"));
      phase_transition(Phase::Rest);
      _proc_stat.current_rest = 0;
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
  _proc_stat.phase_start = now();
  _proc_stat.current_step = Step::Start;
  _proc_stat.need_confirmation = false;
  _transient_proc_stat.user_confirmed = false;
  // no need to call update_eeprom() here because every phase transition also comes with
  // a step transition which then calls update_eeprom();
}

void BrewProcess::step_transition(Step next_step)
{
  _proc_stat.current_step = next_step;
  if(next_step == Step::UserPrompt)
  {
    _proc_stat.need_confirmation = true;
    _transient_proc_stat.user_confirmed = false;
  }
  else if(next_step == Step::Terminated)
  {
    _proc_stat.running = false;
    _proc_stat.phase_char = '-';
  }
  else
  {
    _proc_stat.need_confirmation = false;
    _transient_proc_stat.user_confirmed = false;    
  }
  update_eeprom(true);
}

void BrewProcess::update_target_temp()
{
  switch(_proc_stat.current_phase)
  {
  case Phase::MashIn:
    _proc_stat.target_temp = _receipe.mash_in_temp;
    break;
  case Phase::Rest:
    _proc_stat.target_temp = _receipe.rest_temp[_proc_stat.current_rest];
    break;
  case Phase::MashOut:
    _proc_stat.target_temp = -1;
    break;
  case Phase::SecondWash:
    _proc_stat.target_temp = _receipe.second_wash_temp;
    break;
  case Phase::Boil:
    _proc_stat.target_temp = _config.heater_cook_temp;
    break;
  default:
    _proc_stat.target_temp = -1;
  }
} 

void BrewProcess::update_display_name()
{
  switch(_proc_stat.current_phase)
  {
  case Phase::MashIn:
    strcpy_P(_transient_proc_stat.display_name, PSTR("Einmaischen"));
    break;
  case Phase::Rest:
    sprintf_P(_transient_proc_stat.display_name, PSTR("Rast #%d"), _proc_stat.current_rest + 1);
    break;
  case Phase::MashOut:
    strcpy_P(_transient_proc_stat.display_name, PSTR("Abmaischen"));
    break;
  case Phase::Boil:
    strcpy_P(_transient_proc_stat.display_name, PSTR("Kochen"));
    break;
  case Phase::SecondWash:
    strcpy_P(_transient_proc_stat.display_name, PSTR("Nachguss"));
    break;
  default:
   strcpy_P(_transient_proc_stat.display_name, PSTR("undef"));
   break;
  }
  
  switch(_proc_stat.current_step)
  {
  case Step::Heat:
    strcat_P(_transient_proc_stat.display_name, PSTR("/Heizen"));
    break;
  case Step::Hold:
    strcat_P(_transient_proc_stat.display_name, PSTR("/Halten"));
    break;
  default:
    break;
  }
}

// ====================================================
// heater management
// ====================================================
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
    _rf_sender->sendUnit(RC_OUTLET_HEATER, false);
  }
}

void BrewProcess::turn_on_heater()
{
  if (!_heater_stat.on)
  {
    _heater_stat.on = true;
    _heater_stat.last_on = millis();
    _rf_sender->sendUnit(RC_OUTLET_HEATER, true);
  }
}

// ====================================================
// temperature sensor management
// ====================================================
void BrewProcess::read_temp_sensor ()
{
#ifdef MOCK_TEMP_SENSOR
  _temp_stat.current_temp = 42.0F;
  return;
#endif
  if(_temp_stat.error_count > 3 && _temp_stat.error_count < 5)
  {
    setup_temp_sensor();
  }
  else if (_temp_stat.error_count >= 5)
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
  _proc_stat.rest_start = now();
}

bool BrewProcess::is_rest_timer_over()
{
  return now() - _proc_stat.rest_start > _proc_stat.current_rest_duration;
}

// ====================================================
// saving and recovering state to/from EEPROM
// ====================================================
void BrewProcess::recover_eeprom_state()
{
  unsigned long mgx;
  byte* p = (byte*)(void*)&mgx;
  int mgx_offset = EEPROM_PROC_STAT_OFFSET + sizeof(_proc_stat) - sizeof(mgx);  

  read_eeprom(p, sizeof(mgx), mgx_offset);
  
  if (mgx == _proc_stat.VERSION)
  {
    debug(F("Reading EEPROM"));
    p = (byte*)(void*)&_proc_stat;
    read_eeprom(p, sizeof(_proc_stat), EEPROM_PROC_STAT_OFFSET);
    if(_proc_stat.running) // load previous receipe only if process was interrupted
    {
      p = (byte*)(void*)&_receipe;
      read_eeprom(p, sizeof(_receipe), EEPROM_RECEIPE_OFFSET);
      setTime(_proc_stat.eeprom_saved_timestamp);
      update_process();
    }

    debug_state();
  }
}

void BrewProcess::update_eeprom(bool force)
{
  if(now() - _proc_stat.eeprom_saved_timestamp > EEPROM_UPDATE_INTERVAL || force)
  {
    debug(F("Updating EEPROM"));
    debug_state();
    _proc_stat.eeprom_saved_timestamp = now();

    write_eeprom((byte *)(void *)&_proc_stat, sizeof(_proc_stat), EEPROM_PROC_STAT_OFFSET);
    write_eeprom((byte *)(void *)&_receipe, sizeof(_receipe), EEPROM_RECEIPE_OFFSET);
  }    
}

void BrewProcess::read_eeprom(byte* data, int size, int offset)
{
  byte* p = data;
  
  for(int i = 0; i < size; i++)
  {
    *p = EEPROM.read(offset + i);
    p++;
  }
}

void BrewProcess::write_eeprom(byte* data, int size, int offset)
{
  unsigned long start = millis();
  int i = 0;
  byte* p = data;
  byte b;
  int unchanged = 0;
  int changed = 0;
  for(; i < size; i++)
  {
    if ((b = EEPROM.read(offset + i)) == *p)
    {
      unchanged++;
    }
    else
    {
      changed++;
      EEPROM.write(offset + i, *p);
      if ((b = EEPROM.read(offset + i)) != *p)
      {
        char buffer[20];
        sprintf_P(buffer, PSTR("ERR: Exp %02x Was %02x"), *p, b);
        debug(buffer);
      }
    }
    p++;
  }
  debugnnl(F("Wrote ")); debugnnl(i); debugnnl(F(" bytes in ")); debugnnl(millis() - start); debugnnl(F(" ms, "));
  debugnnl(unchanged); debugnnl(F(" unchanged and ")); debugnnl(changed); debug(F(" changed"));
}

void BrewProcess::debug_state()
{
  /*
  debugnnl(F("  running ")); debug(_proc_stat.running);
  debugnnl(F("  phase_char ")); debug(_proc_stat.phase_char);

  debugnnl(F("  process_start ")); debug(_proc_stat.process_start);
  debugnnl(F("  phase_start ")); debug(_proc_stat.phase_start);
  debugnnl(F("  rest_start ")); debug(_proc_stat.rest_start);

  debugnnl(F("  current_phase ")); debug(_proc_stat.current_phase);
  debugnnl(F("  current_step ")); debug(_proc_stat.current_step);
  debugnnl(F("  current_rest ")); debug(_proc_stat.current_rest);
  debugnnl(F("  current_rest_duration ")); debug(_proc_stat.current_rest_duration);

  debugnnl(F("  need_confirmation ")); debug(_proc_stat.need_confirmation);

  debugnnl(F("  target_temp ")); debug(_proc_stat.target_temp);

  debugnnl(F("  eeprom_saved_timestamp ")); debug(_proc_stat.eeprom_saved_timestamp);

  debugnnl(F("  VERSION ")); debug(_proc_stat.VERSION);
  */
}

