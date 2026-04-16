# Arduino Load Cell Weight Counting System

> **Sistem Verifikasi Kuantitas Part Mikro dalam Kemasan Plastik**
> Menggunakan Density Map Estimation dan Weight Counting Berbasis Edge Computing

[![Platform](https://img.shields.io/badge/Platform-Arduino%20Uno%20%7C%20Nano-blue)]()
[![Load Cell](https://img.shields.io/badge/Load%20Cell-5kg-green)]()
[![License](https://img.shields.io/badge/License-MIT-yellow)]()

---

## 📋 Overview

This repository contains the **weight counting subsystem** for a micro-part quantity verification system. It works alongside a **computer vision (Density Map Estimation)** model to provide dual-verification of part counts in plastic packaging.

### How It Works

```
┌─────────────────┐     USB/Serial     ┌─────────────────┐
│   Arduino Uno   │ ─── JSON data ───▶ │    Edge PC      │
│   + HX711       │                    │    (Python)      │
│   + 5kg Cell    │                    │                  │
│                 │                    │  ┌─────────────┐ │
│  Measures weight│                    │  │ Weight Count │ │
│  Estimates count│                    │  │      +       │ │
│  Filters noise  │                    │  │  AI Count    │ │
│  Detects stable │                    │  │      =       │ │
│                 │                    │  │  OK / NG     │ │
└─────────────────┘                    │  └─────────────┘ │
                                       └─────────────────┘
```

### Key Features

| Feature | Description |
|---------|-------------|
| 🔬 **High Precision** | 3-stage noise filtering for sub-gram accuracy |
| 📊 **Smart Counting** | Weight-based part count estimation |
| ⚡ **Fast Response** | Stable reading in < 3 seconds |
| 🔗 **JSON Output** | Easy integration with Python/Edge PC |
| 🛠️ **Interactive Calibration** | Step-by-step calibration via Serial Monitor |
| 🛡️ **Error Handling** | Sensor detection, drift warnings, outlier rejection |
| 🤖 **AI Fusion Ready** | Designed to work with DME (Density Map Estimation) |

---

## 🔌 Hardware Requirements

| Component | Specification | Qty |
|-----------|--------------|:---:|
| Arduino | Uno or Nano (ATmega328P) | 1 |
| Load Cell | 5kg capacity, strain gauge | 1 |
| HX711 | 24-bit ADC amplifier module | 1 |
| USB Cable | Type-A to Type-B (Uno) or Mini-USB (Nano) | 1 |
| Wires | Jumper wires, female-to-female | 4 |
| Mounting | Rigid base plate + weighing platform | 1 |

### Wiring Diagram

```
┌──────────────┐          ┌──────────┐          ┌──────────────┐
│   LOAD CELL  │          │  HX711   │          │   ARDUINO    │
│              │          │          │          │              │
│   Red   (E+)├──────────┤E+        │          │              │
│   Black (E-)├──────────┤E-     DT ├──────────┤D3            │
│   White (A-)├──────────┤A-    SCK ├──────────┤D2            │
│   Green (A+)├──────────┤A+    VCC ├──────────┤5V            │
│              │          │     GND ├──────────┤GND           │
└──────────────┘          └──────────┘          └──────────────┘
```

> **⚠️ Note:** Load cell wire colors may vary by manufacturer. Check your datasheet!

---

## 📁 Repository Structure

```
arduino-loadcell-system/
│
├── src/
│   └── main.ino                    # Main Arduino firmware
│
├── config/
│   └── config.h                    # All configurable parameters
│
├── docs/
│   ├── calibration_5kg.md          # Step-by-step calibration guide
│   ├── noise_handling.md           # Noise filtering documentation
│   └── integration_with_ai.md     # AI/DME integration guide
│
├── examples/
│   └── serial_output_example.txt   # Example serial output
│
├── python_integration/
│   └── read_serial.py              # Python reader + AI fusion
│
└── README.md                       # This file
```

---

## 🚀 Quick Start

### Step 1: Install Arduino IDE

Download from [arduino.cc](https://www.arduino.cc/en/software)

### Step 2: Install HX711 Library

1. Open Arduino IDE
2. Go to **Sketch → Include Library → Manage Libraries**
3. Search for **"HX711"**
4. Install **"HX711 by Bogdan Necula"** (version 0.7.5 or later)

### Step 3: Wire the Hardware

Connect the load cell → HX711 → Arduino as shown in the wiring diagram above.

### Step 4: Upload Firmware

1. Open `src/main.ino` in Arduino IDE
2. Select your board: **Tools → Board → Arduino Uno** (or Nano)
3. Select your port: **Tools → Port → COMx**
4. Click **Upload** (→ button)

### Step 5: Calibrate

1. Open **Serial Monitor** (Tools → Serial Monitor)
2. Set baud rate to **9600**
3. Set line ending to **Newline**
4. Send **`C`** to enter calibration mode
5. Follow the on-screen instructions
6. See [docs/calibration_5kg.md](docs/calibration_5kg.md) for detailed guide

### Step 6: Update Configuration

After calibration, update `config/config.h` with your new values:

```cpp
float CALIBRATION_FACTOR = -420.5312;  // Your calibrated value
float WEIGHT_PER_PART = 2.3;          // Your part weight in grams
float PACKAGING_WEIGHT = 1.5;         // Your packaging weight in grams
```

Re-upload the firmware after updating.

---

## ⚙️ Configuration

All parameters are in `config/config.h`:

### Pin Configuration

```cpp
const int HX711_DT_PIN = 3;    // Data pin
const int HX711_SCK_PIN = 2;   // Clock pin
```

### Part Settings

```cpp
float WEIGHT_PER_PART = 2.3;    // Grams per part
float PACKAGING_WEIGHT = 1.5;   // Empty package weight
```

### Filter Tuning

```cpp
const int MOVING_AVG_WINDOW = 15;       // Samples to average
const float EMA_ALPHA = 0.1;            // Smoothing factor
const float OUTLIER_THRESHOLD = 5.0;    // Outlier rejection (grams)
const float STABILITY_THRESHOLD = 0.3;  // Stability delta (grams)
const int STABILITY_COUNT = 8;          // Consecutive stable readings
```

### Timing

```cpp
const unsigned long SAMPLE_INTERVAL_MS = 50;   // Read every 50ms
const unsigned long OUTPUT_INTERVAL_MS = 500;   // Output every 500ms
```

---

## 📡 Serial Commands

Send these characters via Serial Monitor or from Python:

| Command | Action |
|:-------:|--------|
| `C` | Enter interactive calibration mode |
| `T` | Tare (zero) the scale |
| `R` | Reset the system |
| `S` | Print current system status |
| `D` | Debug mode info |

---

## 📤 JSON Output Format

The system outputs one JSON line every 500ms:

```json
{
  "weight_gross": 152.34,
  "weight_net": 150.84,
  "count_estimate": 65.58,
  "count_rounded": 66,
  "stable": true,
  "zero_drift": false,
  "weight_per_part": 2.30
}
```

| Field | Type | Description |
|-------|------|-------------|
| `weight_gross` | float | Total weight including packaging (g) |
| `weight_net` | float | Weight of parts only (g) |
| `count_estimate` | float | Exact count from weight |
| `count_rounded` | int | Nearest integer count |
| `stable` | bool | `true` when reading is stable |
| `zero_drift` | bool | `true` if zero point has drifted |
| `weight_per_part` | float | Configured part weight |

### Error Output

```json
{"error": "HX711_NOT_CONNECTED", "stable": false, "weight_gross": 0.00}
```

---

## 🐍 Python Integration

### Install Dependencies

```bash
pip install pyserial
```

### Run the Reader

```bash
# Default (COM3)
python python_integration/read_serial.py

# Custom port
python python_integration/read_serial.py --port COM5

# With expected count
python python_integration/read_serial.py --port COM3 --expected 50

# Verbose output
python python_integration/read_serial.py --verbose
```

### Command Line Options

| Option | Default | Description |
|--------|---------|-------------|
| `--port` | COM3 | Serial port |
| `--baud` | 9600 | Baud rate |
| `--tolerance` | 3 | Max count difference for OK |
| `--expected` | None | Expected part count |
| `--log` | True | Enable CSV logging |
| `--verbose` | False | Verbose output |

### AI Fusion Decision

The Python script makes OK/NG decisions by comparing:

```
Weight Count (from Arduino) vs AI Count (from DME model)

If |AI_count - Weight_count| < tolerance → ✓ OK
If |AI_count - Weight_count| ≥ tolerance → ✗ NG
```

See [docs/integration_with_ai.md](docs/integration_with_ai.md) for full integration details.

---

## 🔧 Noise Filtering Pipeline

The firmware applies **three-stage filtering** optimized for the 5kg load cell:

```
Raw Reading → Outlier Rejection → Moving Average → EMA → Stability Check → Output
```

| Stage | Effect |
|-------|--------|
| Outlier Rejection | Discards spikes > 5g from average |
| Moving Average (15) | Reduces noise by ~4× |
| EMA (α=0.1) | Additional ~2× noise reduction |
| Stability (8 readings) | Only outputs when reading is steady |

**Result:** ±2.0g raw noise → ±0.25g filtered output

See [docs/noise_handling.md](docs/noise_handling.md) for detailed analysis.

---

## ⚠️ Important: 5kg Load Cell Considerations

This system uses a **5kg load cell** to measure parts weighing **< 3g each**. This is an intentional design choice for the capstone project, but it requires special attention:

### Challenges

| Issue | Impact | Mitigation |
|-------|--------|------------|
| Low resolution | ~0.5g noise vs 2.3g part | 3-stage filtering |
| Temperature sensitivity | ±1g per 10°C change | Warm-up + re-tare |
| Mechanical vibration | Can mask small weight changes | Rubber feet + stability detection |
| Zero drift | Slow baseline shift | Automatic drift detection |

### Best Practices

1. **Always tare** before each measurement session
2. **Allow 5 minutes warm-up** after power-on
3. **Don't blow air** over the load cell
4. **Center the load** on the weighing platform
5. **Use a rigid mounting** — no flexible surfaces
6. **Keep wires short** (< 30cm from HX711 to load cell)

### If Accuracy Is Not Sufficient

Consider upgrading to a **500g or 1kg load cell** for better resolution. The firmware will work without changes — just recalibrate.

---

## 🐛 Troubleshooting

| Problem | Possible Cause | Solution |
|---------|---------------|----------|
| "HX711 not detected" | Wiring error | Check DT→D3, SCK→D2, VCC→5V, GND→GND |
| Readings jump ±10g+ | Loose connections | Secure all wires, check solder joints |
| Weight slowly drifts | Temperature change | Re-tare (send `T`), allow warm-up |
| Always reads 0.00g | Wrong calibration factor | Run calibration (send `C`) |
| Negative weight | Inverted calibration | Try negating the calibration factor |
| Very slow response | Filter window too large | Reduce `MOVING_AVG_WINDOW` in config.h |
| Too much noise | Filter window too small | Increase `MOVING_AVG_WINDOW` to 20+ |
| Count is always wrong | Wrong weight per part | Measure 10 parts, divide by 10, update config |

---

## 📚 Documentation

| Document | Description |
|----------|-------------|
| [calibration_5kg.md](docs/calibration_5kg.md) | Complete calibration tutorial |
| [noise_handling.md](docs/noise_handling.md) | Noise filtering deep dive |
| [integration_with_ai.md](docs/integration_with_ai.md) | DME model integration guide |
| [serial_output_example.txt](examples/serial_output_example.txt) | Example output |

---

## 🤝 System Integration

This subsystem is part of a larger project:

```
┌────────────────────────────────────────────────────┐
│              Complete System                        │
│                                                    │
│  ┌──────────────┐     ┌──────────────────────────┐│
│  │ Weight Module │     │   Vision Module          ││
│  │ (This Repo)  │     │   (CAPSTONE--Density-    ││
│  │              │     │    Mapping-)             ││
│  │ Arduino      │     │                          ││
│  │ + HX711      │     │   Camera + PyTorch DME   ││
│  │ + Load Cell  │     │   Density Map → Count    ││
│  └──────┬───────┘     └────────────┬─────────────┘│
│         │                          │               │
│         └──────────┬───────────────┘               │
│                    │                               │
│           ┌────────┴────────┐                      │
│           │  Fusion Engine  │                      │
│           │  (read_serial.py│                      │
│           │   + DME output) │                      │
│           │                 │                      │
│           │  OK / NG        │                      │
│           └─────────────────┘                      │
└────────────────────────────────────────────────────┘
```

---

## 📄 License

This project is developed as a capstone project for academic purposes.

---

## 👨‍💻 Authors

Capstone Project Team — Sixth Semester 2026
