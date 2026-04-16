# Noise Handling Guide — 5kg Load Cell

## 🎯 The Problem

Using a **5kg capacity load cell** to measure parts weighing **< 3 grams** presents significant noise challenges:

```
Full Scale (5000g)  ████████████████████████████████████████░░░
Actual Range (~200g) ██░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
Single Part (2.3g)   ░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░

You are using only 0.05% of the load cell's resolution per part!
```

**Typical noise level** on a 5kg cell: ±0.5g to ±2.0g  
**Single part weight**: 2.3g  
**This means noise can equal the weight of an entire part!**

---

## 🔧 Noise Sources & Solutions

### 1. Electrical Noise

| Source | Solution | Implemented? |
|--------|----------|:---:|
| Power supply fluctuations | Use USB power (regulated) | ✓ |
| Long wires acting as antenna | Keep wires short (< 30cm) | Manual |
| Nearby motors/relays | Shield cables, add ferrite beads | Manual |
| Ground loops | Single ground point | Manual |
| HX711 internal noise | Averaging + filtering | ✓ |

### 2. Mechanical Noise

| Source | Solution | Implemented? |
|--------|----------|:---:|
| Table vibration | Rubber isolation feet | Manual |
| Air currents / drafts | Enclose the scale | Manual |
| Load cell mounting flex | Rigid metal base plate | Manual |
| Eccentric loading | Center parts on cell | Manual |
| Thermal expansion | Allow warm-up time | Manual |

### 3. Software Noise

| Source | Solution | Implemented? |
|--------|----------|:---:|
| Single-sample noise | Moving Average (15 samples) | ✓ |
| Sudden spikes | Outlier Rejection (5g threshold) | ✓ |
| High-frequency jitter | EMA Smoothing (α=0.1) | ✓ |
| Unstable reporting | Stability Detection (8 readings) | ✓ |

---

## 📊 Software Filtering Pipeline

Our firmware applies **three-stage filtering** to every reading:

```
Raw HX711 Reading
       │
       ▼
┌──────────────────┐
│  Outlier Rejection│  ← Discard if |reading - average| > 5g
│  (Gate Filter)    │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Moving Average   │  ← Average of last 15 readings
│  (Window=15)      │     Smooth out random noise
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Exponential      │  ← Weighted blend of new and old
│  Moving Average   │     α = 0.1 (highly smoothed)
│  (EMA, α=0.1)    │
└────────┬─────────┘
         │
         ▼
┌──────────────────┐
│  Stability Check  │  ← Must stay within ±0.3g
│  (8 consecutive)  │     for 8 consecutive readings
└────────┬─────────┘
         │
         ▼
   Stable JSON Output
```

### Filter Details

#### Moving Average

```
Window size: 15 samples (at 20 Hz = 0.75 seconds of data)

Effect:  Noise ±2.0g → Noise ±0.5g (4x reduction)

Formula: avg = sum(last 15 readings) / 15
```

**Trade-off:** Larger window = smoother reading but slower response  
**Recommended range:** 10–20 for this application

#### Exponential Moving Average (EMA)

```
Alpha: 0.1 (10% new, 90% old)

Effect: Additional 2x noise reduction on top of moving average

Formula: ema = α × new_value + (1 - α) × old_ema
```

**Why use EMA on top of Moving Average?**
- Moving average treats all samples equally
- EMA gives more weight to recent data while maintaining smoothness
- The combination provides excellent noise rejection with reasonable response time

#### Outlier Rejection

```
Threshold: 5.0g

Any reading that differs from the current filtered
value by more than 5g is discarded completely.

This catches:
  - Bumps to the scale
  - Electrical spikes
  - Sensor glitches
```

#### Stability Detection

```
Threshold: ±0.3g change
Required:  8 consecutive stable readings
Time:      8 × 50ms sampling = 400ms minimum stable time

Only when this condition is met will the system report
the reading as "stable": true in the JSON output.
```

---

## 📈 Noise Performance Comparison

Measured with empty load cell over 100 readings:

| Stage | Noise (±g) | Improvement |
|-------|-----------|-------------|
| Raw HX711 | ±2.0 | baseline |
| After Moving Average (15) | ±0.5 | 4× better |
| After EMA (α=0.1) | ±0.25 | 8× better |
| After Stability Gate | ±0.3 | Only stable readings pass |

---

## ⚙️ Tuning the Filters

Edit `config/config.h` to adjust filter parameters:

### For More Stability (slower response)

```cpp
const int MOVING_AVG_WINDOW = 20;      // Was 15
const float EMA_ALPHA = 0.05;          // Was 0.1
const float STABILITY_THRESHOLD = 0.2;  // Was 0.3
const int STABILITY_COUNT = 12;         // Was 8
```

### For Faster Response (more noise)

```cpp
const int MOVING_AVG_WINDOW = 8;       // Was 15
const float EMA_ALPHA = 0.3;           // Was 0.1
const float STABILITY_THRESHOLD = 0.5;  // Was 0.3
const int STABILITY_COUNT = 5;          // Was 8
```

### For Very Light Parts (< 1g each)

```cpp
const int MOVING_AVG_WINDOW = 25;      // Maximum smoothing
const float EMA_ALPHA = 0.05;          // Very slow response
const float STABILITY_THRESHOLD = 0.15; // Very tight stability
const int STABILITY_COUNT = 15;         // Must be very stable
const float OUTLIER_THRESHOLD = 3.0;    // Tighter outlier gate
```

---

## 🏗️ Mechanical Best Practices

### Load Cell Mounting

```
        ┌─────────────────────┐  ← Weighing Platform
        │   ▓▓▓▓▓▓▓▓▓▓▓▓▓   │     (rigid, flat)
        └──────────┬──────────┘
                   │
            ┌──────┴──────┐
            │  LOAD CELL  │  ← Firmly bolted
            └──────┬──────┘
                   │
        ┌──────────┴──────────┐
        │   BASE PLATE        │  ← Heavy, rigid
        │   (aluminum/steel)  │
        └─┬──┬──────────┬──┬─┘
          │  │          │  │
         🔴 🔴        🔴 🔴  ← Rubber isolation feet
```

### Key Mounting Rules

1. **Bolt, don't glue** the load cell to both plates
2. **Use M4/M5 bolts** with washers to spread the load
3. **Level the base** — uneven surface causes drift
4. **Minimum 3mm aluminum** for base and platform
5. **Add rubber feet** (durometer 40-60 Shore A)

### Cable Management

```
✗ BAD:  Long dangling wires near motors
✓ GOOD: Short, shielded cables, tied down, away from EMI sources
```

---

## 🌡️ Temperature Effects

5kg load cells have a typical temperature coefficient of **±0.02% / °C**.

This means:
- **10°C change → ±1g drift** (nearly half a part!)
- Allow **5 minutes warm-up** before calibrating
- Re-tare at the start of each measurement session
- Don't operate near heat sources or air conditioning vents

---

## 📉 When Noise Is Still Too High

If after all optimizations the noise is still unacceptable:

1. **Hardware upgrade:** Consider a **500g or 1kg** load cell for better resolution
2. **Add a second stage:** Use a hardware low-pass RC filter before HX711
3. **Increase HX711 gain:** Use channel A at 128x gain (already default)
4. **Shield everything:** Put the HX711 in a metal enclosure
5. **Use twisted pair wires** between load cell and HX711
