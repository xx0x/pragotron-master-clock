/*
 * Pragotron Master Clock
 * - for Pragotron IPJ0612 and others.
 * - based on SAMD21G18A microcontroller and DS3231MZ precise RTC clock
 *
 * Author: Vaclav Mach
 * https://github.com/xx0x/pragotron-master-clock
 * 
 * Sketch uses Adafruit Trinket M0 as the core
 * Modified Trinket M0 bootloader (you can use the Trinket one as well)
 */

#define PBMask(_x) (1 << ((_x)&0x1f))
#define PGrp(_x) ((_x) >> 5)

#define SET_HIGH(n) PORT->Group[PGrp(n)].OUTSET.reg = PBMask(n)
#define SET_LOW(n) PORT->Group[PGrp(n)].OUTCLR.reg = PBMask(n)
#define SET_PIN(n, h) h ? SET_HIGH(n) : SET_LOW(n)
#define TOGGLE_PIN(n) PORT->Group[PGrp(n)].OUTTGL.reg = PBMask(n)
#define SET_OUTPUT(n) PORT->Group[PGrp(n)].DIRSET.reg = PBMask(n)
#define SET_INPUT(n) \
  PORT->Group[PGrp(n)].DIRCLR.reg = PBMask(n); \
  PORT->Group[PGrp(n)].PINCFG[n % 32].reg |= PORT_PINCFG_INEN
#define SET_INPUT_PULLUP(n) \
  SET_INPUT(n); \
  PORT->Group[PGrp(n)].PINCFG[n % 32].reg |= PORT_PINCFG_PULLEN; \
  SET_HIGH(n); \
  PORT->Group[PGrp(n)].PINCFG[n % 32].reg |= PORT_PINCFG_INEN

#define IS_LOW(n) !(PORT->Group[PGrp(n)].IN.reg & (1 << (n % 32)))
#define IS_HIGH(n) !IS_LOW(n)
#define IS_PRESSED(n) IS_LOW(n)


#define PIN_LEDX PIN_PA23

#define PIN_BOOST_ENABLE PIN_PA09
#define PIN_TIMER_INTERRUPT PIN_PA07

#define PIN_OUT_A PIN_PA00
#define PIN_OUT_B PIN_PA01
#define PIN_OUT_C PIN_PA02
#define PIN_OUT_D PIN_PA03

#define PIN_BTN_FAST PIN_PA16
#define PIN_BTN_XFAST PIN_PA17
#define PIN_BTN_MINUTE_RESET PIN_PA18

bool lastState = false;
uint32_t lastTime = 0;

uint32_t clearAt = 0;
uint32_t ledOffAt = 0;

uint16_t secondCounter = 0;
uint8_t minuteCounter = 0;
#define SECOND_COUNT_TO 32768
#define MINUTE_COUNT_TO 60

bool minuteTrigger = false;

enum MODE {
  MODE_NORMAL,
  MODE_FAST,
  MODE_XFAST,
  MODE_COUNT
};

enum BUTTON {
  BUTTON_FAST,
  BUTTON_XFAST,
  BUTTON_MINUTE_RESET,
  BUTTON_COUNT
};

MODE mode = MODE_NORMAL;
bool buttonState[BUTTON_COUNT];
bool prevButtonState[BUTTON_COUNT];
uint8_t buttonsPins[BUTTON_COUNT] = { PIN_BTN_FAST, PIN_BTN_XFAST, PIN_BTN_MINUTE_RESET };

void setup() {
  SET_OUTPUT(PIN_LEDX);
  SET_OUTPUT(PIN_BOOST_ENABLE);
  SET_OUTPUT(PIN_OUT_A);
  SET_OUTPUT(PIN_OUT_B);
  SET_OUTPUT(PIN_OUT_C);
  SET_OUTPUT(PIN_OUT_D);
  SET_INPUT(PIN_PA07);

  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    SET_INPUT_PULLUP(buttonsPins[i]);
    buttonState[i] = false;
    prevButtonState[i] = false;
  }

  clear();
  setBoost(false);
  setLed(false);

  // Start precise RTC counter
  attachInterrupt(3, receiveTimeInterrupt, FALLING);  // PA07 is D3 on Trinket
}

void receiveTimeInterrupt() {
  secondCounter = (secondCounter + 1) % SECOND_COUNT_TO;
  if (secondCounter == 0) {
    minuteCounter = (minuteCounter + 1) % MINUTE_COUNT_TO;
    if (minuteCounter == 0) {
      minuteTrigger = true;
    }
  }
}

void clear() {
  SET_LOW(PIN_OUT_A);
  SET_LOW(PIN_OUT_B);
  SET_LOW(PIN_OUT_C);
  SET_LOW(PIN_OUT_D);
}

void setPositive() {
  clear();
  SET_HIGH(PIN_OUT_A);
  SET_HIGH(PIN_OUT_D);
}

void setNegative() {
  clear();
  SET_HIGH(PIN_OUT_B);
  SET_HIGH(PIN_OUT_C);
}

void setLed(bool state) {
  SET_PIN(PIN_LEDX, state);
}

void setBoost(bool state) {
  SET_PIN(PIN_BOOST_ENABLE, state);
}

bool isBoost() {
  return IS_HIGH(PIN_BOOST_ENABLE);
}

void resetCounters() {
  minuteCounter = 0;
  secondCounter = 0;
}

void readButtons() {
  for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
    prevButtonState[i] = buttonState[i];
    buttonState[i] = IS_LOW(buttonsPins[i]);
  }
}

bool justPressed(BUTTON button) {
  return !prevButtonState[button] && buttonState[button];
}

void loop() {
  delay(10);  // debounce

  readButtons();

  if (justPressed(BUTTON_FAST)) {
    if (mode == MODE_FAST) {
      mode = MODE_NORMAL;
      resetCounters();
    } else {
      mode = MODE_FAST;
    }
  }

  if (justPressed(BUTTON_XFAST)) {
    if (mode == MODE_XFAST) {
      mode = MODE_NORMAL;
      resetCounters();
    } else {
      mode = MODE_XFAST;
    }
  }

  if (justPressed(BUTTON_MINUTE_RESET)) {
    mode = MODE_NORMAL;
    resetCounters();
  }

  uint32_t interval = 0;
  uint32_t onLength = 2000;

  switch (mode) {
    case MODE_FAST:
      interval = 1000;
      break;
    case MODE_XFAST:
      interval = 250;
      break;
  }

  if ((mode == MODE_NORMAL && minuteTrigger) || ((mode == MODE_FAST || mode == MODE_XFAST) && (millis() - lastTime >= interval || lastTime > millis()))) {

    if (!isBoost()) {
      setBoost(true);
    }
    if (lastState) {
      setNegative();
    } else {
      setPositive();
    }
    minuteTrigger = false;
    lastTime = millis();
    clearAt = (lastTime + onLength);
    ledOffAt = (lastTime + interval / 2);
    setLed(true);
    lastState = !lastState;
  }

  if (ledOffAt && millis() > ledOffAt) {
    setLed(false);
    ledOffAt = 0;
  }
  if (clearAt && millis() > clearAt) {
    clear();
    clearAt = 0;
    if (mode == MODE_NORMAL) {
      setBoost(false);
    }
  }
}
