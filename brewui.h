#ifndef __UI_H
#define __UI_H

#define __DEBUG
#include "debug.h"
#include "encoder.h"
#include "Arduino.h"
#include "brauwerkstatt.h"

/**
 * UI owns the LCD and the encoder
 */
class BrewUi
{
public:

  BrewUi(BrewProcess* brew_proc, LiquidCrystal_I2C* lcd, byte enc_pin_a, byte enc_pin_b, byte enc_pin_switch);
  void init();
  void update_ui();
  void encoder_isr();

private:
  char _lines[4][21];
  int _menu_ptr = 1;
  bool _is_menu_showing = false;

  BrewProcess* _brew_process;
  LiquidCrystal_I2C* _lcd;
  Encoder* _encoder;

  unsigned long last_print_ui = 0;

  void display_process_state(bool serial);
  void display_menu(bool serial);
  void create_status_line(char* strbuf);

  void update_line(const char* newText, int line);
  void update_menu_ptr(int menu_idx);
};

#endif /* __UI_H */
