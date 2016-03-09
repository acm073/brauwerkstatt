#include <LiquidCrystal_I2C.h>
#include <Time.h>
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
  clear_screen();
  update_line_P(PSTR(" Brauwerkstatt v0.1"), 1);
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
    if(_current_screen == Screen::Error)
    {
      // only process events if error screen is already showing
      if (clicks > 0)
      {
        _brew_process->resetError();
      }
    }
    set_screen(Screen::Error);          
    display_error(serial);
  }
  else if(_brew_process->hasWarning())
  {
    
  }
  else if (_brew_process->isRunning())
  {
    set_screen(Screen::Process);

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
    set_screen(Screen::Menu);

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

void BrewUi::set_screen(Screen s)
{
  if (_current_screen != s)
  {
    clear_screen();
    _current_screen = s;
  }
}

void BrewUi::display_error(bool serial)
{
  update_line(_brew_process->getErrorMessage(), 1);
  update_line_P(PSTR("        Ok?"), 3);

  if (serial)
  {
    output_serial("", _lines[1], "", _lines[3]);
  }
}

void BrewUi::display_menu(bool serial)
{
  char buffer[21];

  create_status_line(buffer);
  _lcd->setCursor(0,0);
  _lcd->print(buffer);

  update_line_P(PSTR(" Maischen"), 1);
  update_line_P(PSTR(" Nachguss"), 2);
  update_line_P(PSTR(" Kochen"), 3);
  update_menu_ptr(_menu_ptr);

//  _lcd->print(F("  Manuell"));

  if(serial)
  {
    output_serial(buffer, _lines[1], _lines[2], _lines[3]);
  }
}

void BrewUi::output_serial(char* line0, char* line1, char* line2, char* line3)
{
  if (strcmp(_serial_lines[1], line1) != 0
   || strcmp(_serial_lines[2], line2) != 0
   || strcmp(_serial_lines[3], line3) != 0)
  {
    debug(F("--------------------"));
    debug(line0);
    debug(line1);
    debug(line2);
    debug(line3);
    debug(F("--------------------"));
    strcpy(_serial_lines[1], _lines[1]);
    strcpy(_serial_lines[2], _lines[2]);
    strcpy(_serial_lines[3], _lines[3]);
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
    unsigned long phase_running = now() - _brew_process->phaseStart();
    unsigned long phase_rest = _brew_process->phaseRest();
    
    int phase_min = numberOfMinutes(phase_running);
    int phase_sec = numberOfSeconds(phase_running);
    
    if (phase_rest > 0)
    {
      int rest_min = numberOfMinutes(phase_rest);
      int rest_sec = numberOfSeconds(phase_rest);
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
  unsigned long proc_running = now() - _brew_process->procStart();
  int run_hrs = numberOfHours(proc_running);
  int run_min = numberOfMinutes(proc_running);
  int run_sec = numberOfSeconds(proc_running);
  float current_temp = _brew_process->getCurrentTemp();
  int temp_deg = (int)current_temp;
  int temp_frac = ((int)(current_temp * 10.0F)) % 10;
  sprintf_P(strbuf, PSTR("%02d:%02d:%02d  %c %c %02d.%d%cC"),
      run_hrs, run_min, run_sec,
      _brew_process->getPhaseChar(),
      _brew_process->heaterOn() ? 'H' : ' ',
      temp_deg, temp_frac, (char)223);  
}

/**
 * Clear the screen. Sets the background buffer for LCD and Serial to empty strings.
 */
void BrewUi::clear_screen()
{
  for (int i = 0; i < LCD_LINES; i++)
  {
    _lines[i][0] = '\0';
    _serial_lines[i][0] = '\0';
  }
  _lcd->clear();
}

/**
 * Update the screen in lines.
 */
void BrewUi::update_screen(char** lines, byte progmem_mask)
{
  // print out all lines
  for (int i = 0; i < LCD_LINES; i++)
  {
    if(progmem_mask & (1 << i))
    {
      update_line_P(lines[i], i);
    }
    else
    {
      update_line(lines[i], i);
    }
  }

  // update menu pointer
  
  // update scroll symbols
}

void BrewUi::update_line_P(const char* newText, int line_idx)
{
  char buffer[21];
  strcpy_P(buffer, newText);
  update_line(buffer, line_idx);  
}

void BrewUi::update_line(char* buffer, int line_idx)
{
  if (_menu_ptr == line_idx)
  {
    buffer[0] = '>';
  }
  _lcd->setCursor(0, line_idx);
  _lcd->print(buffer);

  // erase rest of line
  for (int pos = strlen(buffer); pos < strlen(_lines[line_idx]); pos++)
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
    if (_current_screen == Screen::Menu)
    {
      if (strlen(_lines[menu_idx]) == 0)
      {
        _lines[menu_idx][1] = '\0';
      }
      _lines[menu_idx][0] = '>';
    }
    // clear menu char at current position
    _lcd->setCursor(0, _menu_ptr);
    _lcd->print(" ");
    _lcd->setCursor(0, menu_idx);
    _lcd->print(">");
    _menu_ptr = menu_idx;
  }
}


