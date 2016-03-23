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
  update_line_P(PSTR(" Brauwerkstatt v1.0"), 1, false, false, false);
  delay(1500);
}

void BrewUi::update_ui()
{
  int clicks = _encoder->readClicks();
  int steps = _encoder->readSteps();
  int holds = _encoder->readHolds();

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
    display_error();
  }
  else if(_brew_process->hasWarning())
  {
    
  }
  else if (_brew_process->isRunning())
  {
    set_screen(Screen::Process);

    if(holds > 0)
    {
      _brew_process->stop_process();
    }
    else if (_brew_process->needConfirmation())
    {
      if (clicks > 0)
      {
          _brew_process->confirm();
      }
    }
    display_process_state();
  }
  else // menu mode
  {
    set_screen(Screen::Menu);

    if(steps != 0)
    {
      _menu_ptr += steps;
      if (_menu_ptr < 1) _menu_ptr = 1;
      if (_menu_ptr > 3) _menu_ptr = 3;
    }
    else if(clicks > 0)
    {
      debugnnl(F("Menu item selected at index "));
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
        break;
      case 3:
        _brew_process->load_receipe();
        _brew_process->start_boil_process();
        break;
      default:
        break;
      }
    }
    display_menu();
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

void BrewUi::display_error()
{
  update_line_P(PSTR(""), 0, false, false, false);
  update_line(_brew_process->getMessage(), 1, false, false, false);
  update_line_P(PSTR(""), 2, false, false, false);
  update_line_P(PSTR("      Ok?"), 3, false, false, false);
}

void BrewUi::display_menu()
{
  char buffer[21];

  create_status_line(buffer);

  update_line(buffer, 0, false, false, false);
  update_line_P(PSTR(" Maischen"), 1, false, false, _menu_ptr == 1);
  update_line_P(PSTR(" Nachguss"), 2, false, false, _menu_ptr == 2);
  update_line_P(PSTR(" Kochen"), 3, false, false, _menu_ptr == 3);
}

void BrewUi::display_process_state()
{

  // first line: status
  char buffer[21];
  create_status_line(buffer);
  update_line(buffer, 0, false, false, false);

  // second line: status name
  update_line(_brew_process->getDisplayName(), 1, false, false, false);

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
    sprintf(buffer, "");
  }
  update_line(buffer, 2, false, false, false);

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
  update_line(buffer, 3, false, false, false);
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
    memset(_lines[i], ' ', LCD_COLS);
    _lines[i][LCD_COLS] = '\0';
  }
  _lcd->clear();
}

void BrewUi::update_line_P(const char* buffer, int line_idx, bool scrollUp, bool scrollDown, bool menuPtr)
{
  char buf[21];
  strcpy_P(buf, buffer);
  update_line(buf, line_idx, scrollUp, scrollDown, menuPtr);
}

void BrewUi::update_line(const char* buffer, int line_idx, bool scrollUp, bool scrollDown, bool menuPtr)
{
  char full_line[LCD_COLS + 1];
  memset(full_line, ' ', LCD_COLS);
  full_line[LCD_COLS] = '\0';
  memcpy(full_line, buffer, strlen(buffer));

  if(scrollUp)
  {
    full_line[LCD_COLS - 1] = (char)8;
  }
  if(scrollDown)
  {
    full_line[LCD_COLS - 1] = (char)9;
  }
  if(menuPtr)
  {
    full_line[0] = '>';
  }

  bool diff = false;
  for(int i = 0; i < LCD_COLS; i++)
  {
    if (full_line[i] != _lines[line_idx][i])
    {
      diff = true;
      _lcd->setCursor(i, line_idx);
      _lcd->print(full_line[i]);
    }
  }
  memcpy(_lines[line_idx], full_line, LCD_COLS + 1);

  if (diff)
  {
    bool dump_serial = false;
    switch(_current_screen)
    {
    case Screen::Menu:
      // dump menu screen only if something except the first line changed
      // first line contains clock
      if(line_idx != 0)
      {
        dump_serial = true;
      }
      break;
    case Screen::Process:
      // dump process screen every 5 secs to serial
      if(millis() - last_print_ui > 5000)
      {
        dump_serial = true;
        last_print_ui = millis();
      }
      break;
    default:
      // splash screen and error/warning are dumped whenever something changes
      dump_serial = true;
    }
    if (dump_serial)
    {
      output_serial();
    }
  }

}

void BrewUi::output_serial()
{
  debug(F("--------------------"));
  for (int i = 0; i < LCD_LINES; i++)
  {
    debug(_lines[i]);    
  }
  debug(F("--------------------"));
}
