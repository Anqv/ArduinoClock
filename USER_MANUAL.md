# User Manual — Arduino Digital Clock with Alarm

---

## Overview

This clock displays the current time in hours, minutes, and seconds on an 8-digit LED display. It has a daily alarm with a buzzer, and four push buttons for all settings. Time is kept by a battery-backed DS3231 real-time clock module, so the clock remembers the correct time even when power is disconnected.

---

## The Display

In normal operation the display shows the current time in **HH-MM-SS** format:

```
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ H │ H │ - │ M │ M │ - │ S │ S │
└───┴───┴───┴───┴───┴───┴───┴───┘
  12   -   3   0   -   4   5
  └─ hours ┘   └─ mins ┘   └─ sec ┘
```

**Alarm indicator:** A small dot (decimal point) lights up in the bottom-right corner of the last digit whenever the alarm is switched **ON**.

---

## The Buttons

| Button | Label | Position |
|--------|-------|----------|
| **MODE** | Cycle / Confirm | Leftmost |
| **UP** | Increase | Second from left |
| **DOWN** | Decrease | Third from left |
| **ALARM** | Alarm ON/OFF · Stop | Rightmost |

---

## Setting the Time

1. Press **MODE** once — the display switches to time-setting and the **hours** digits begin to blink.
2. Press **UP** or **DOWN** to set the correct hour (0–23, wraps around).
3. Press **MODE** again — the **minutes** digits begin to blink.
4. Press **UP** or **DOWN** to set the correct minutes (0–59).
5. Press **MODE** again — the **seconds** digits begin to blink.
6. Press **UP** or **DOWN** to set the correct seconds (0–59).
7. Press **MODE** one final time — the new time is saved and the clock returns to normal operation.

> **Tip:** If you make no button press for **10 seconds** while in any setting screen, the clock saves whatever values are shown and returns to normal automatically.

---

## Setting the Alarm

The alarm setting follows immediately after the time setting in the same MODE cycle. You can either:

- Continue from step 7 above by pressing **MODE** again instead of confirming, **or**
- Press **MODE** four times from the normal display to skip past the time fields directly to the alarm.

**Alarm display:**

```
┌───┬───┬───┬───┬───┬───┬───┬───┐
│ A │   │ H │ H │ - │ M │ M │   │
└───┴───┴───┴───┴───┴───┴───┴───┘
```

The letter **A** on the left confirms you are editing the alarm time.

1. The **alarm hour** digits blink — press **UP** / **DOWN** to set the hour (0–23).
2. Press **MODE** — the **alarm minute** digits blink — press **UP** / **DOWN** to set the minutes (0–59).
3. Press **MODE** — the alarm time is saved and the clock returns to normal.

> The alarm always triggers at **second 00** of the set minute.

---

## Turning the Alarm ON and OFF

Press the **ALARM** button at any time (even while setting the time). Each press toggles the alarm between ON and OFF.

- **Alarm ON** → the small dot in the bottom-right corner of the display is lit.
- **Alarm OFF** → the dot is dark.

---

## When the Alarm Rings

The buzzer sounds a repeating beep pattern at the set time. To stop it, press the **ALARM** button.

If the alarm is not stopped manually it will silence itself automatically after **60 seconds**.

> The alarm triggers once per day at the set time. It will ring again the following day at the same time as long as the alarm remains switched ON.

---

## Full Settings Cycle at a Glance

```
Normal display
    │
    ▼  [MODE]
Hours blink        ← UP / DOWN to adjust
    │
    ▼  [MODE]
Minutes blink      ← UP / DOWN to adjust
    │
    ▼  [MODE]
Seconds blink      ← UP / DOWN to adjust
    │
    ▼  [MODE]
Alarm hours blink  ← UP / DOWN to adjust
    │
    ▼  [MODE]
Alarm mins blink   ← UP / DOWN to adjust
    │
    ▼  [MODE]  ── or wait 10 s ──
Normal display  (time & alarm saved)
```

---

## Error Indication

If the display shows a row of **E** letters flashing at startup, the clock module (DS3231) is not responding. Check that all wiring connections are secure and restart the power.

---

## Battery Backup

The DS3231 module contains a small CR2032 coin cell that keeps the time running even when the main power is off. When the battery is exhausted the clock will still work when powered, but will lose the correct time if unplugged. Replace the coin cell on the DS3231 module to restore backup operation.
