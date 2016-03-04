#ifndef BW_ENCODER_H_
#define BW_ENCODER_H_

#include "Arduino.h"

class Encoder
{
public:
  Encoder(int pinA, int pinB, int pinSwitch);

  /*
   * return number of encoder steps and reset internal counter back to 0
   */
  int readSteps();

  /*
   * return number of clicks and reset internal counter back to 0
   */
  int readClicks();

  /*
   * return number of button holds and reset internal counter back to 0
   * a button hold is a long press of >2sec and is counted as soon as the button
   * has been held long enough
   */
  int readHolds();

  /*
   * the encoder service routine, which is supposed to be called by a timer interrupt
   */
  void service();

private:
  volatile byte _encoder_last = LOW;

  volatile int _steps = 0;
  volatile int _clicks = 0;
  volatile int _holds = 0;

  // encoder state variables
  volatile unsigned long _enc_last_micros = 0;  // time of last encoder event, for debouncing
  volatile byte _enc_a_last_state = LOW;

  // button state variables
  volatile unsigned long _btn_last_micros = 0;  // time of last button event, for debouncing and HOLD detection
  volatile bool _btn_held = false;  // state goes to true as soon as button remains pressed
  volatile unsigned long _btn_down_micros = 0;  // last button down event
  volatile int _btn_last_state = HIGH;  // last button state


  int _enc_a_pin;
  int _enc_b_pin;
  int _switch_pin;

  void encoderService();
  void buttonService();
  void serialService();
};

#endif /* BW_ENCODER_H_ */
