# Calibration Guide for 5kg Load Cell

## ⚠️ Why Calibration is Critical

This system uses a **5kg capacity load cell** to measure parts that weigh **< 3 grams each**. This means:

- The useful range is only **0.1% - 2%** of the load cell's full capacity
- Small calibration errors can cause **large counting errors**
- Temperature changes and mechanical stress affect accuracy more significantly

**Example:** A 1% error on a 5kg cell = 50g error = **~21 parts miscounted** (at 2.3g/part)

---

## 🛠️ What You Need

| Item | Purpose |
|------|---------|
| Arduino Uno/Nano | Microcontroller |
| HX711 module | Amplifier/ADC |
| 5kg Load Cell | Weighing sensor |
| Known calibration weight | 100g or 200g recommended |
| Stable flat surface | Mounting platform |
| USB cable + computer | Serial communication |
| Arduino IDE | Upload firmware |

---

## 📋 Step-by-Step Calibration

### Step 0: Hardware Setup

```
Load Cell Wiring to HX711:
  Red    → E+
  Black  → E-
  White  → A-
  Green  → A+

HX711 to Arduino:
  DT  → Pin D3
  SCK → Pin D2
  VCC → 5V
  GND → GND
```

> **Important:** Mount the load cell on a **rigid, flat surface**. Any flex in the mounting will cause inaccurate readings.

### Step 1: Upload Firmware

1. Open `src/main.ino` in Arduino IDE
2. Install the **HX711 library** by Bogdan Necula:
   - Go to `Sketch → Include Library → Manage Libraries`
   - Search for "HX711"
   - Install "HX711 by Bogdan Necula" (version 0.7.5+)
3. Select your board (Uno/Nano) and port
4. Upload the firmware

### Step 2: Open Serial Monitor

1. Open Serial Monitor (`Tools → Serial Monitor`)
2. Set baud rate to **9600**
3. Set line ending to **Newline**
4. You should see the startup messages

### Step 3: Enter Calibration Mode

1. Type **`C`** and press Enter
2. You'll see the calibration menu:

```
========================================
  CALIBRATION MODE
  5kg Load Cell Calibration
========================================

Commands:
  1 — Tare (zero) the scale
  2 — Calibrate with known weight
  3 — Show current raw value
  4 — Set calibration factor manually
  5 — Test current calibration
  6 — Set weight per part
  7 — Set packaging weight
  0 — Exit calibration mode
```

### Step 4: Tare the Scale

1. **Remove ALL items** from the load cell
2. Type **`1`** and press Enter
3. Press any key when ready
4. Wait for "Tare complete. Zero point set."

### Step 5: Calibrate with Known Weight

1. Type **`2`** and press Enter
2. Place your **known calibration weight** on the center of the load cell
   - Use 100g or 200g for best results
   - Place gently, don't drop it
3. Press any key when the weight is placed
4. Wait for the raw value reading
5. Enter the known weight in grams (e.g., `100`)
6. The system will calculate and display the calibration factor

```
[CAL] ✓ Calibration complete!
[CAL] New calibration factor: -420.5312

[CAL] ⚠ UPDATE config.h:
[CAL]   float CALIBRATION_FACTOR = -420.5312;
```

### Step 6: Verify Calibration

1. Type **`5`** to test calibration
2. Place your known weight on the scale
3. Verify that all 10 readings show the correct weight (±0.5g)

```
  Reading 1: 100.12 g
  Reading 2: 99.98 g
  Reading 3: 100.05 g
  ...
```

> **If readings are off:** Repeat Step 4 and Step 5

### Step 7: Update config.h

1. Type **`0`** to exit calibration mode
2. Open `config/config.h`
3. Update the calibration factor:

```cpp
float CALIBRATION_FACTOR = -420.5312;  // Your new value
```

4. Re-upload the firmware

### Step 8: Set Part Parameters

While in calibration mode:

1. Type **`6`** to set the weight per part
   - Weigh 10 identical parts
   - Divide total weight by 10
   - Enter the average (e.g., `2.3`)

2. Type **`7`** to set packaging weight
   - Weigh an empty plastic package
   - Enter the weight (e.g., `1.5`)

---

## 🎯 Improving Calibration Accuracy

### For 5kg Load Cell with Micro Parts

Since you're measuring parts < 3g on a 5kg cell, follow these tips:

#### Mechanical Improvements

| Tip | Impact |
|-----|--------|
| Mount load cell on rigid metal plate | ★★★ Critical |
| Use rubber feet to isolate vibration | ★★★ Critical |
| Center the load on the load cell | ★★ Important |
| Avoid drafts/wind near the scale | ★★ Important |
| Keep temperature stable | ★ Helpful |
| Use shortest possible wires | ★ Helpful |

#### Software Improvements (already implemented)

| Feature | Setting | Purpose |
|---------|---------|---------|
| Moving average | Window = 15 | Smooth noisy readings |
| EMA smoothing | Alpha = 0.1 | Additional noise filtering |
| Outlier rejection | 5.0g threshold | Ignore spike readings |
| Stability detection | 0.3g / 8 readings | Only report stable values |

#### Calibration Weight Selection

Best practice for 5kg load cell calibrating for micro parts:

| Calibration Weight | Suitability |
|-------------------|-------------|
| 50g | Good — closer to measurement range |
| 100g | Best — good balance of accuracy and stability |
| 200g | Good — more stable reading |
| 500g | Fair — too far from measurement range |
| 1000g+ | Poor — not representative of actual use |

> **Recommendation:** Use **100g** calibration weight for best results

---

## 🔄 Re-Calibration Schedule

| Event | Action |
|-------|--------|
| Every session start | Tare the scale (send `T`) |
| Weekly | Full calibration with known weight |
| After moving the system | Full calibration |
| After temperature change > 10°C | Full calibration |
| If readings seem drifting | Check zero drift, re-tare or recalibrate |

---

## 🐛 Troubleshooting

### "HX711 not detected"
- Check wiring: DT→D3, SCK→D2
- Verify HX711 has power (VCC→5V, GND→GND)
- Try different DT/SCK pins and update config.h

### Readings are jumping ±10g or more
- Check load cell wiring (Red→E+, Black→E-, White→A-, Green→A+)
- Ensure rigid mounting (flexing causes noise)
- Increase `MOVING_AVG_WINDOW` in config.h to 20+

### Readings slowly drift
- Allow 5 minutes warm-up time
- Re-tare before each measurement batch
- Keep ambient temperature stable

### All readings show 0.00g
- Calibration factor may be wrong
- Run calibration mode (send `C`)
- Check if load cell is properly stressed (deforming)

### Negative weight readings
- Calibration factor sign is wrong — try negating it
- Load cell wires may be swapped (swap A+ and A-)

---

## 📏 Calibration Factor Quick Reference

Typical calibration factors for common 5kg load cells:

| Load Cell Type | Factor Range |
|---------------|-------------|
| Generic Chinese 5kg | -380 to -450 |
| Sparkfun 5kg | -400 to -430 |
| TAL220 | -410 to -440 |

> **Note:** Your specific factor depends on your exact load cell and HX711 module. Always calibrate with a known weight.
