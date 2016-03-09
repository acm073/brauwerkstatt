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
  enum Screen { Error, Warning, Menu, Process, Splash };

  Screen _current_screen = Screen::Splash;
  // display is a 4x20 LCD and is managed in lines.
  char _lines[LCD_LINES][LCD_COLS + 1];
  char _serial_lines[LCD_LINES][LCD_COLS + 1];
  int _menu_ptr = 1;

  BrewProcess* _brew_process;
  LiquidCrystal_I2C* _lcd;
  Encoder* _encoder;

  unsigned long last_print_ui = 0;

  void display_process_state(bool serial);
  void display_menu(bool serial);
  void display_error(bool serial);
  void display_warning(bool serial);

  void clear_screen();
  void set_screen(Screen s);
  void create_status_line(char* strbuf);
  void update_screen(char** lines, byte progmem_mask);
  void update_line_P(const char* newText, int line);
  void update_line(char* buffer, int line);
  void update_menu_ptr(int menu_idx);

  void output_serial(char* line0, char* line1, char* line2, char* line3);
};

#endif /* __UI_H */
