#include <LiquidCrystal_I2C.h>
#include "encoder.h"
#include "brewproc.h"
#include "encoder.h"
#include "brewui.h"
#include "brauwerkstatt.h"

BrewUi::BrewUi(BrewProcess* brew_proc, LiquidCrystal_I2C* lcd, byte enc_pin_a, byte enc_pin_b, byte enc_pin_switch)
{
  _brew_process = brew_proc;
  _encoder = new Encoder(enc_pin_a, enc_pin_b, enc_pin_switch);

  _lcd = lcd;
}

void BrewUi::init()
{
  _lcd->init();
  _lcd->backlight();
  update_line(" Brauwerkstatt v0.1", 1);
  delay(1500);
}

void BrewUi::update_ui()
{
  int clicks = _encoder->readClicks();
  int steps = _encoder->readSteps();
  int holds = _encoder->readHolds();
  int menu_idx = _menu_ptr;

  // show output on serial only every 5 seconds
  bool serial = false;
  if(millis() - last_print_ui > 5000)
  {
    serial = true;
    last_print_ui = millis();
  }

  if (_brew_process->hasError())
  {
    
  }
  else if(_brew_process->hasWarning())
  {
    
  }
  else if (_brew_process->isRunning())
  {
    _is_menu_showing = false;
    if (_brew_process->needConfirmation())
    {
      if (clicks > 0)
      {
          _brew_process->confirm();
      }
    }
    display_process_state(serial);
  }
  else // menu mode
  {
    if (!_is_menu_showing)
    {
      // force a screen clear when entering menu mode
      _lcd->clear();
      _is_menu_showing = true;
    }

    if(steps != 0)
    {
      menu_idx += steps;
      if (menu_idx < 1) menu_idx = 1;
      if (menu_idx > 3) menu_idx = 3;
    }
    else if(clicks > 0)
    {
      debugnnl(F("Menu item selected at index"));
      debug(_menu_ptr);
      switch(_menu_ptr)
      {
      case 1:
        _brew_process->load_receipe();
        _brew_process->start_mash_process();
        break;
      case 2:
        _brew_process->load_receipe();
        _brew_process->start_second_wash_process();
      }
    }

    display_menu(serial);
    update_menu_ptr(menu_idx);
  }
}

void BrewUi::encoder_isr()
{
  _encoder->service();
}

void BrewUi::display_menu(bool serial)
{
  char buffer[21];

  create_status_line(buffer);
  _lcd->setCursor(0,0);
  _lcd->print(buffer);

  update_line(PSTR(" Maischen"), 1);
  update_line(PSTR(" Nachguss"), 2);
  update_line(PSTR(" Kochen"), 3);
//  _lcd->print(F("  Manuell"));

  if(serial)
  {
    if (strcmp(_serial_lines[1], _lines[1]) != 0
     || strcmp(_serial_lines[2], _lines[2]) != 0
     || strcmp(_serial_lines[3], _lines[3]) != 0)
    {
      debug(F("--------------------"));
      debug(buffer);
      debug(_lines[1]);
      debug(_lines[2]);
      debug(_lines[3]);
      debug(F("--------------------"));
      strcpy(_serial_lines[1], _lines[1]);
      strcpy(_serial_lines[2], _lines[2]);
      strcpy(_serial_lines[3], _lines[3]);
    }
  }
}

void BrewUi::display_process_state(bool serial)
{
  if(serial) debug(F("--------------------"));

  // first line: status
  char buffer[30];
  create_status_line(buffer);
  while(strlen(buffer) < LCD_COLS)
  {
    strcat(buffer, " ");
  }
  _lcd->setCursor(0,0);
  _lcd->print(buffer);
  if (serial) debug(buffer);

  // second line: Process step
  if (strlen(_brew_process->getStepName()) > 0)
  {
    sprintf_P(buffer, PSTR("%s/%s"), _brew_process->getPhaseName(), _brew_process->getStepName());    
  }
  else
  {
    sprintf_P(buffer, PSTR("%s"), _brew_process->getPhaseName());
  }
  while (strlen(buffer) < LCD_COLS)
  {
    strcat(buffer, " ");
  }
  _lcd->setCursor(0,1);
  _lcd->print(buffer);
  if (serial) debug(buffer);

  // third line: target temp
  if(_brew_process->getTargetTemp() > 0)
  {
    float targ_temp = _brew_process->getTargetTemp();
    int temp_deg = (int)targ_temp;
    int temp_frac = ((int)(targ_temp * 10.0F)) % 10;
    sprintf_P(buffer, PSTR("Soll: %02d.%d%cC"), temp_deg, temp_frac, (char)223);  
  }
  else
  {
    sprintf(buffer, " ");
  }
  while(strlen(buffer) < LCD_COLS)
  {
    strcat(buffer, " ");
  }
  _lcd->setCursor(0,2);
  _lcd->print(buffer);
  if(serial) debug(buffer);

  // fourth line: either Prompt or Timings
  if (_brew_process->needConfirmation())
  {
    sprintf_P(buffer, PSTR("        %s"), _brew_process->getPrompt());
  }
  else
  {
    unsigned long phase_running = millis() - _brew_process->phaseStart();
    unsigned long phase_rest = _brew_process->phaseRest();
    
    int phase_min = (int)(phase_running / 60000L);
    int phase_sec = (int)(phase_running / 1000L % 60L);
    
    if (phase_rest > 0)
    {
      int rest_min = (int)(phase_rest / 60000L);
      int rest_sec = (int)(phase_rest / 1000L % 60L);
      sprintf_P(buffer, PSTR("%02d:%02d (Rest %02d:%02d)"), phase_min, phase_sec, rest_min, rest_sec);
    }
    else
    {
      sprintf_P(buffer, PSTR("%02d:%02d"), phase_min, phase_sec);
    }
  }
  while(strlen(buffer) < LCD_COLS)
  {
    strcat(buffer, " ");
  }
  _lcd->setCursor(0,3);
  _lcd->print(buffer);
  if (serial) debug(buffer);
  if (serial) debug(F("--------------------"));
  Serial.flush();
}

void BrewUi::create_status_line(char* strbuf)
{
  unsigned long proc_running = millis() - _brew_process->procStart();
  int run_hrs = (int)(proc_running / 60000L / 60L);
  int run_min = (int)(proc_running / 60000L % 60L);
  int run_sec = (int)(proc_running / 1000L % 60L);
  float current_temp = _brew_process->getCurrentTemp();
  int temp_deg = (int)current_temp;
  int temp_frac = ((int)(current_temp * 10.0F)) % 10;
  sprintf_P(strbuf, PSTR("%02d:%02d:%02d  %c %c %02d.%d%cC"),
      run_hrs, run_min, run_sec,
      _brew_process->getPhaseChar(),
      _brew_process->heaterOn() ? 'H' : ' ',
      temp_deg, temp_frac, (char)223);  
}

void BrewUi::update_line(const char* newText, int line_idx)
{
  char buffer[21];
  strcpy_P(buffer, newText);
  
  if (line_idx + 1 > LCD_LINES)
  {
    debug(F("Invalid line index"));
    return;
  }

  if (_is_menu_showing && line_idx == _menu_ptr)
  {
    buffer[0] = '>';
  }

  _lcd->setCursor(0, line_idx);
  _lcd->print(buffer);

  // erase rest of line
  for (int pos = strlen(newText); pos < strlen(_lines[line_idx]); pos++)
  {
    _lcd->setCursor(pos, line_idx);
    _lcd->print(" ");
  }
  
  strcpy(_lines[line_idx], buffer);
}

void BrewUi::update_menu_ptr(int menu_idx)
{
  if(_menu_ptr != menu_idx)
  {
    // clear menu char at current position
    _lcd->setCursor(0, _menu_ptr);
    _lcd->print(" ");
    _lcd->setCursor(0, menu_idx);
    _lcd->print(">");
    _menu_ptr = menu_idx;
  }
}


