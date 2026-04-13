/*
 ╔══════════════════════════════════════════════════════════════════╗
 ║          ARDUINO MEGA 2560 — DIGITAL CLOCK WITH ALARM            ║
 ╚══════════════════════════════════════════════════════════════════╝

 ── REAL-TIME OSCILLATOR ──────────────────────────────────────────
   DS3231 — temperature-compensated crystal oscillator (TCXO)
   Accuracy: ±2 ppm  (~1 minute drift per year)
   Interface: I²C
   Keeps time during power-off via CR2032 coin cell on the module.

 ── BILL OF MATERIALS ────────────────────────────────────────────
   • Arduino Mega 2560
   • DS3231 RTC module (ZS-042 or similar, includes CR2032 holder)
   • MAX7219 8-digit 7-segment LED module
   • 4 × momentary push-buttons (normally open)
   • 1 × active buzzer (3–5 V)

 ── WIRING ───────────────────────────────────────────────────────
   MAX7219 module       Arduino Mega 2560
   ─────────────────    ─────────────────
   VCC              →   5 V
   GND              →   GND
   DIN              →   Pin 51  (MOSI)
   CLK              →   Pin 52  (SCK)
   CS               →   Pin 53  (SS)

   DS3231 module        Arduino Mega 2560
   ─────────────────    ─────────────────
   VCC              →   5 V  (module regulator handles 3.3 V internally)
   GND              →   GND
   SDA              →   Pin 20  (hardware I²C)
   SCL              →   Pin 21  (hardware I²C)

   Buttons (connect between listed pin and GND; internal pull-up used):
   BTN_MODE         →   Pin 2   — cycle settings / confirm save
   BTN_UP           →   Pin 3   — increment active field
   BTN_DOWN         →   Pin 4   — decrement active field
   BTN_ALARM        →   Pin 5   — toggle alarm ON/OFF  |  stop ringing

   Active buzzer:
   (+) positive     →   Pin 6
   (−) negative     →   GND

   NOTE: For a passive buzzer replace the HIGH/LOW writes with
         tone(PIN_BUZZ, 2000) / noTone(PIN_BUZZ).

 ── DISPLAY LAYOUT (8 digits, position 0 = LEFT-most) ────────────
   Time / edit-time mode:
     [0:H₁][1:H₂][2:─][3:M₁][4:M₂][5:─][6:S₁][7:S₂]
     e.g.   1  2  ─  3  0  ─  4  5   →   "12-30-45"
     DP on digit 7 (rightmost) lights when alarm is ENABLED.

   Edit-alarm mode:
     [0:A][1: ][2:H₁][3:H₂][4:─][5:M₁][6:M₂][7: ]
     e.g.   A     0  7  ─  3  0     →   "A 07-30 "
     Edited field blinks at 2 Hz.

   NOTE: If digits appear left↔right reversed on your module,
         change DIGIT_REVERSED to true at the top of this file.

 ── USER INTERFACE ───────────────────────────────────────────────
   BTN_MODE  (press once)
     • NORMAL mode           → enter settings (snapshot RTC → edit buffer)
     • SET_HH                → advance to SET_MM
     • SET_MM                → advance to SET_SS
     • SET_SS                → advance to SET_ALHH  (alarm hours)
     • SET_ALHH              → advance to SET_ALMM  (alarm minutes)
     • SET_ALMM              → SAVE to DS3231 → return to NORMAL

   BTN_UP / BTN_DOWN
     • Increment / decrement the currently blinking field (wraps).

   BTN_ALARM
     • NORMAL / setting mode → toggle alarm ON / OFF
     • While alarm is ringing → silence the alarm immediately

   AUTO-SAVE: after 10 seconds without a button press while in any
              SET mode the clock saves the edit buffer and returns to
              NORMAL automatically.

 ── REQUIRED LIBRARIES (install via Arduino Library Manager) ─────
   • LedControl   by Eberhard Fahle
   • RTClib        by Adafruit
*/

#include <Wire.h>
#include <LedControl.h>
#include <RTClib.h>

// ═══════════════════════════════════════════════════════════════
//  CONFIGURATION  — adjust to your hardware
// ═══════════════════════════════════════════════════════════════

// MAX7219
static const uint8_t PIN_DIN   = 51;
static const uint8_t PIN_CLK   = 52;
static const uint8_t PIN_CS    = 53;

// Buttons  (active LOW — wire to GND, internal pull-up is used)
static const uint8_t PIN_MODE  = 2;
static const uint8_t PIN_UP    = 3;
static const uint8_t PIN_DOWN  = 4;
static const uint8_t PIN_ALARM = 5;

// Buzzer
static const uint8_t PIN_BUZZ  = 6;

// Display orientation:
//   false → digit 0 is the LEFT-most  (common for MAX7219 modules)
//   true  → digit 0 is the RIGHT-most (flip if digits appear reversed)
static const bool DIGIT_REVERSED = false;

// Brightness  0 (dim) … 15 (brightest)
static const uint8_t BRIGHTNESS = 10;

// ─── Timing constants ──────────────────────────────────────────
static const uint16_t DEBOUNCE_MS   =    50;   // button debounce
static const uint16_t BLINK_MS      =   500;   // blink half-period (ms)
static const uint32_t INACTIVITY_MS = 10000UL; // auto-save & exit (ms)
static const uint32_t ALARM_MAX_MS  = 60000UL; // alarm rings max 60 s

// ═══════════════════════════════════════════════════════════════
//  STATE MACHINE
// ═══════════════════════════════════════════════════════════════
enum ClockMode : uint8_t {
  MODE_NORMAL   = 0,
  MODE_SET_HH   = 1,   // edit clock hours
  MODE_SET_MM   = 2,   // edit clock minutes
  MODE_SET_SS   = 3,   // edit clock seconds
  MODE_SET_ALHH = 4,   // edit alarm hours
  MODE_SET_ALMM = 5,   // edit alarm minutes
  MODE_COUNT    = 6
};

// ═══════════════════════════════════════════════════════════════
//  GLOBAL STATE
// ═══════════════════════════════════════════════════════════════
LedControl lc(PIN_DIN, PIN_CLK, PIN_CS, 1);
RTC_DS3231 rtc;

ClockMode gMode = MODE_NORMAL;

// Edit buffer (staged values — written to DS3231 on save)
uint8_t gEditH = 12, gEditM = 0, gEditS = 0;

// Alarm
uint8_t gAlmH  =  7, gAlmM  = 0;
bool    gAlmOn = false;

// Ringing state
bool     gRinging   = false;
uint32_t gRingStart = 0;

// Display blink
uint32_t gLastBlink = 0;
bool     gBlinkOn   = true;

// Settings inactivity timer
uint32_t gLastActivity = 0;

// ─── Button structure ──────────────────────────────────────────
struct Btn {
  uint8_t  pin;
  bool     rawPrev;      // previous raw reading
  bool     stablePrev;   // previous stable state
  uint32_t edgeMs;       // time of last raw change
  bool     fired;        // single-shot "pressed" event
};

static Btn gBtns[4] = {
  { PIN_MODE,  HIGH, HIGH, 0, false },
  { PIN_UP,    HIGH, HIGH, 0, false },
  { PIN_DOWN,  HIGH, HIGH, 0, false },
  { PIN_ALARM, HIGH, HIGH, 0, false },
};
#define BTN_MODE  0
#define BTN_UP    1
#define BTN_DOWN  2
#define BTN_ALARM 3

// ═══════════════════════════════════════════════════════════════
//  FORWARD DECLARATIONS
// ═══════════════════════════════════════════════════════════════
void pollButtons();
void handleInput();
void adjustField(int8_t delta);
void saveAndExit();
void checkAlarm();
void updateBuzzer();
void refreshDisplay();
void showTimeFace(uint8_t h, uint8_t m, uint8_t s,
                  bool blinkH, bool blinkM, bool blinkS, bool almDot);
void showAlarmFace(uint8_t h, uint8_t m, bool blinkH, bool blinkM);
void writeDigit(uint8_t logicalPos, uint8_t val, bool dp = false);
void writeDash(uint8_t logicalPos);
void writeChar(uint8_t logicalPos, char c, bool dp = false);
void writeBlank(uint8_t logicalPos, bool dp = false);
uint8_t physPos(uint8_t logicalPos);   // handle orientation flip
void fatalError();

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  // MAX7219 — wake, set brightness, clear
  lc.shutdown(0, false);
  lc.setIntensity(0, BRIGHTNESS);
  lc.clearDisplay(0);

  // Buttons with internal pull-ups
  for (auto &b : gBtns) {
    pinMode(b.pin, INPUT_PULLUP);
  }

  // Buzzer
  pinMode(PIN_BUZZ, OUTPUT);
  digitalWrite(PIN_BUZZ, LOW);

  // DS3231 RTC
  Wire.begin();
  if (!rtc.begin()) {
    fatalError();   // RTC not found — flash error and halt
  }
  if (rtc.lostPower()) {
    // Seed from sketch compile time; user can correct via buttons
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

// ═══════════════════════════════════════════════════════════════
//  MAIN LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {
  pollButtons();
  handleInput();
  updateBuzzer();    // also calls checkAlarm()
  refreshDisplay();
}

// ═══════════════════════════════════════════════════════════════
//  BUTTON POLLING  (50 ms debounce, falling-edge detection)
// ═══════════════════════════════════════════════════════════════
void pollButtons() {
  uint32_t now = millis();
  for (auto &b : gBtns) {
    bool raw = digitalRead(b.pin);

    // Detect any raw change → restart debounce timer
    if (raw != b.rawPrev) {
      b.edgeMs  = now;
      b.rawPrev = raw;
    }

    // After debounce period the reading is considered stable
    if ((now - b.edgeMs) >= DEBOUNCE_MS) {
      if (b.stablePrev == HIGH && raw == LOW) {
        // Rising stable LOW = falling edge = button pressed
        b.fired = true;
      }
      b.stablePrev = raw;
    }
  }
}

// ═══════════════════════════════════════════════════════════════
//  INPUT HANDLING
// ═══════════════════════════════════════════════════════════════
void handleInput() {
  uint32_t now = millis();

  // ── ALARM button: always processed regardless of mode ────────
  if (gBtns[BTN_ALARM].fired) {
    gBtns[BTN_ALARM].fired = false;
    gLastActivity = now;
    if (gRinging) {
      stopAlarmRinging();
    } else {
      gAlmOn = !gAlmOn;
    }
  }

  // ── Auto-exit settings after inactivity ─────────────────────
  if (gMode != MODE_NORMAL && (now - gLastActivity) >= INACTIVITY_MS) {
    saveAndExit();
    return;
  }

  // ── MODE button ──────────────────────────────────────────────
  if (gBtns[BTN_MODE].fired) {
    gBtns[BTN_MODE].fired = false;
    gLastActivity = now;

    if (gMode == MODE_NORMAL) {
      // Snapshot current RTC time into edit buffer
      DateTime t = rtc.now();
      gEditH = t.hour();
      gEditM = t.minute();
      gEditS = t.second();
      gMode  = MODE_SET_HH;
    } else {
      gMode = static_cast<ClockMode>((uint8_t)gMode + 1);
      if (gMode >= MODE_COUNT) {
        saveAndExit();
      }
    }
  }

  // ── UP / DOWN: only meaningful in edit modes ─────────────────
  if (gMode == MODE_NORMAL) {
    gBtns[BTN_UP].fired   = false;
    gBtns[BTN_DOWN].fired = false;
    return;
  }

  if (gBtns[BTN_UP].fired) {
    gBtns[BTN_UP].fired = false;
    gLastActivity = now;
    adjustField(+1);
  }
  if (gBtns[BTN_DOWN].fired) {
    gBtns[BTN_DOWN].fired = false;
    gLastActivity = now;
    adjustField(-1);
  }
}

// ─── Increment or decrement the currently edited field ────────
void adjustField(int8_t delta) {
  // Wrapping helper: result is always in [0, maxVal]
  auto wrap = [](int16_t v, uint8_t maxVal) -> uint8_t {
    int16_t r = v % (int16_t)(maxVal + 1);
    return (uint8_t)(r < 0 ? r + maxVal + 1 : r);
  };
  switch (gMode) {
    case MODE_SET_HH:   gEditH = wrap((int16_t)gEditH + delta, 23); break;
    case MODE_SET_MM:   gEditM = wrap((int16_t)gEditM + delta, 59); break;
    case MODE_SET_SS:   gEditS = wrap((int16_t)gEditS + delta, 59); break;
    case MODE_SET_ALHH: gAlmH  = wrap((int16_t)gAlmH  + delta, 23); break;
    case MODE_SET_ALMM: gAlmM  = wrap((int16_t)gAlmM  + delta, 59); break;
    default: break;
  }
}

// ─── Write edit buffer to DS3231 and return to normal ────────
void saveAndExit() {
  DateTime cur = rtc.now();
  rtc.adjust(DateTime(cur.year(), cur.month(), cur.day(),
                      gEditH, gEditM, gEditS));
  gMode    = MODE_NORMAL;
  gBlinkOn = true;   // ensure display shows immediately on return
}

// ═══════════════════════════════════════════════════════════════
//  ALARM LOGIC
// ═══════════════════════════════════════════════════════════════
void checkAlarm() {
  if (!gAlmOn || gRinging || gMode != MODE_NORMAL) return;

  DateTime t = rtc.now();
  // Trigger at the exact minute boundary (second == 0)
  if (t.hour() == gAlmH && t.minute() == gAlmM && t.second() == 0) {
    gRinging   = true;
    gRingStart = millis();
  }
}

// Called every loop iteration; also triggers checkAlarm()
void updateBuzzer() {
  checkAlarm();

  if (!gRinging) {
    digitalWrite(PIN_BUZZ, LOW);
    return;
  }

  // Auto-stop after ALARM_MAX_MS
  uint32_t elapsed = millis() - gRingStart;
  if (elapsed >= ALARM_MAX_MS) {
    stopAlarmRinging();
    return;
  }

  // Beep pattern: 300 ms ON / 200 ms OFF  (500 ms cycle)
  uint32_t phase = elapsed % 500UL;
  digitalWrite(PIN_BUZZ, (phase < 300) ? HIGH : LOW);
}

void stopAlarmRinging() {
  gRinging = false;
  digitalWrite(PIN_BUZZ, LOW);
}

// ═══════════════════════════════════════════════════════════════
//  DISPLAY REFRESH
// ═══════════════════════════════════════════════════════════════
void refreshDisplay() {
  // Update blink phase
  uint32_t now = millis();
  if ((now - gLastBlink) >= BLINK_MS) {
    gLastBlink = now;
    gBlinkOn   = !gBlinkOn;
  }

  switch (gMode) {
    case MODE_NORMAL:
    {
      DateTime t = rtc.now();
      showTimeFace(t.hour(), t.minute(), t.second(),
                   false, false, false, gAlmOn);
      break;
    }
    case MODE_SET_HH:
    case MODE_SET_MM:
    case MODE_SET_SS:
      showTimeFace(gEditH, gEditM, gEditS,
                   gMode == MODE_SET_HH,
                   gMode == MODE_SET_MM,
                   gMode == MODE_SET_SS,
                   gAlmOn);
      break;

    case MODE_SET_ALHH:
    case MODE_SET_ALMM:
      showAlarmFace(gAlmH, gAlmM,
                    gMode == MODE_SET_ALHH,
                    gMode == MODE_SET_ALMM);
      break;

    default:
      break;
  }
}

// ═══════════════════════════════════════════════════════════════
//  DISPLAY FACES
// ═══════════════════════════════════════════════════════════════

/*
  Time face (logical positions 0–7, left → right):
    [0:H₁][1:H₂][2:─][3:M₁][4:M₂][5:─][6:S₁][7:S₂]
  almDot: lights DP on position 7 when alarm is enabled
*/
void showTimeFace(uint8_t h, uint8_t m, uint8_t s,
                  bool blinkH, bool blinkM, bool blinkS, bool almDot) {
  bool showH = blinkH ? gBlinkOn : true;
  bool showM = blinkM ? gBlinkOn : true;
  bool showS = blinkS ? gBlinkOn : true;

  if (showH) {
    writeDigit(0, h / 10);
    writeDigit(1, h % 10);
  } else {
    writeBlank(0);
    writeBlank(1);
  }

  writeDash(2);

  if (showM) {
    writeDigit(3, m / 10);
    writeDigit(4, m % 10);
  } else {
    writeBlank(3);
    writeBlank(4);
  }

  writeDash(5);

  if (showS) {
    writeDigit(6, s / 10);
    writeDigit(7, s % 10, almDot);   // DP = alarm indicator
  } else {
    writeBlank(6);
    writeBlank(7, almDot);
  }
}

/*
  Alarm face (logical positions 0–7, left → right):
    [0:A][1: ][2:H₁][3:H₂][4:─][5:M₁][6:M₂][7: ]
*/
void showAlarmFace(uint8_t h, uint8_t m, bool blinkH, bool blinkM) {
  bool showH = blinkH ? gBlinkOn : true;
  bool showM = blinkM ? gBlinkOn : true;

  writeChar(0, 'A');      // 'A' label — steady
  writeBlank(1);

  if (showH) {
    writeDigit(2, h / 10);
    writeDigit(3, h % 10);
  } else {
    writeBlank(2);
    writeBlank(3);
  }

  writeDash(4);

  if (showM) {
    writeDigit(5, m / 10);
    writeDigit(6, m % 10);
  } else {
    writeBlank(5);
    writeBlank(6);
  }

  writeBlank(7);
}

// ═══════════════════════════════════════════════════════════════
//  LOW-LEVEL DISPLAY HELPERS
// ═══════════════════════════════════════════════════════════════

// Convert logical position (0=left … 7=right) to LedControl index
// Set DIGIT_REVERSED = true if digits appear backwards on your module.
uint8_t physPos(uint8_t logicalPos) {
  return DIGIT_REVERSED ? logicalPos : (7 - logicalPos);
}

void writeDigit(uint8_t logicalPos, uint8_t val, bool dp) {
  lc.setDigit(0, physPos(logicalPos), val & 0x0F, dp);
}

void writeDash(uint8_t logicalPos) {
  lc.setChar(0, physPos(logicalPos), '-', false);
}

void writeChar(uint8_t logicalPos, char c, bool dp) {
  lc.setChar(0, physPos(logicalPos), c, dp);
}

void writeBlank(uint8_t logicalPos, bool dp) {
  // setRow with 0x00 (or 0x80 for DP-only) clears the digit
  lc.setRow(0, physPos(logicalPos), dp ? 0x80 : 0x00);
}

// ═══════════════════════════════════════════════════════════════
//  FATAL ERROR  — DS3231 not found at startup
// ═══════════════════════════════════════════════════════════════
void fatalError() {
  lc.clearDisplay(0);
  while (true) {
    for (uint8_t i = 0; i < 8; i++) {
      lc.setChar(0, i, 'E', false);
    }
    delay(500);
    lc.clearDisplay(0);
    delay(500);
  }
}
