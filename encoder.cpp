#define __DEBUG 1

#include "debug.h"
#include "brauwerkstatt.h"
#include "encoder.h"

Encoder::Encoder(int pinA, int pinB, int pinSwitch)
{
  _enc_a_pin = pinA;
  _enc_b_pin = pinB;
  _switch_pin = pinSwitch;

  pinMode(_enc_a_pin, INPUT);
  pinMode(_enc_b_pin, INPUT);
  pinMode(_switch_pin, INPUT);
  // turn on internal pull-up for switch, since the resistor is not mounted on the encoder PCB
  digitalWrite(_switch_pin, HIGH);
}

int Encoder::readSteps()
{
  int result = _steps;
  _steps = 0;
  return result;
}

int Encoder::readClicks()
{
  int result = _clicks;
  _clicks = 0;
  return result;
}

int Encoder::readHolds()
{
  int result = _holds;
  _holds = 0;
  return result;
}

void Encoder::service()
{
#ifdef INPUT_SERIAL
  serialService();
#else
  encoderService();
  buttonService();
#endif
}

void Encoder::encoderService()
{
  unsigned long now = micros();
  // TODO make debounce time (10ms = 10000us) configurable
  if (_enc_last_micros - now >= 10000)
  {
    _enc_last_micros = now;
    byte cur_a = digitalRead(2);
    byte cur_b = digitalRead(4);
    if (_enc_a_last_state == LOW && cur_a == HIGH)
    {
      if(cur_b == LOW)
      {
        debug("step--");
        _steps--;
      }
      else
      {
        debug("step++");
        _steps++;
      }
    }
    _enc_a_last_state = cur_a;
  }

}

void Encoder::buttonService()
{
  unsigned long now = micros();
  // TODO make debounce time (10ms = 10000us) configurable
  if(now - _btn_last_micros > 10000)
  {
    byte state = digitalRead(_switch_pin);
    
    if (_btn_last_state == HIGH && state == LOW)
    {
      debug("click++");
      _btn_down_micros = micros();
      _clicks++;
    }
    else if (_btn_last_state == LOW && state == HIGH)
    {
      _btn_held = false;
    }
    else if (_btn_last_state == LOW && state == LOW && !_btn_held)
    {
      if (micros() - _btn_down_micros > 2000000)
      {      
        debug("holds++");
        _holds++;
        _btn_held = true;
      }
    }

    _btn_last_state = state;
    _btn_last_micros = now;
  }
}

void Encoder::serialService()
{
  // - for encoder left
  // + for encoder right
  // c for encoder click
  // h for encoder hold
  if (Serial.available() > 0)
  {
    byte c = Serial.read();
    debugnnl("Got a char on serial: ");
    debug((char)c);
    switch (c)
    {
    case '-':
      _steps--;
      break;
    case '+':
      _steps++;
      break;
    case 'c':
      _clicks++;
      break;
    case 'h':
      _holds++;
    }
  }
}

