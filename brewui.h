#ifndef __UI_H
#define __UI_H

#undef __DEBUG

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
  enum Screen { Error, Warning, Menu, Process, Splash };

  Screen _current_screen = Screen::Splash;

  // LCD backing buffer
  char _lines[LCD_LINES][LCD_COLS + 1];
  
  int _menu_ptr = 1;

  BrewProcess* _brew_process;
  LiquidCrystal_I2C* _lcd;
  Encoder* _encoder;

  unsigned long last_print_ui = 0;

  void display_process_state();
  void display_menu();
  void display_error();
  void display_warning();

  void clear_screen();
  void set_screen(Screen s);
  void create_status_line(char* strbuf);
  void update_line_P(const char* buffer, int line, bool scrollUp, bool scrollDown, bool menuPtr);
  void update_line(const char* buffer, int line, bool scrollUp, bool scrollDown, bool menuPtr);
  void update_menu_ptr(int menu_idx);

  void output_serial();
};

#endif /* __UI_H */
