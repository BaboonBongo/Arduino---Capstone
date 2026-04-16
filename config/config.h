// ============================================================
// config.h — Configuration for Arduino Load Cell System
// ============================================================
// Project: Sistem Verifikasi Kuantitas Part Mikro
// Load Cell: 5kg capacity
// Amplifier: HX711
// ============================================================

#ifndef CONFIG_H
#define CONFIG_H

// ────────────────────────────────────────────────────────────
// PIN CONFIGURATION
// ────────────────────────────────────────────────────────────
// HX711 Data pin → Arduino Digital Pin 3
const int HX711_DT_PIN = 3;

// HX711 Clock pin → Arduino Digital Pin 2
const int HX711_SCK_PIN = 2;

// ────────────────────────────────────────────────────────────
// CALIBRATION SETTINGS
// ────────────────────────────────────────────────────────────
// Calibration factor: Adjust this after running calibration!
// This value converts raw HX711 readings to grams.
// Run calibration mode and update this value.
// Typical range for 5kg load cell: -400 to -500 (varies per unit)
float CALIBRATION_FACTOR = -420.0;

// Number of readings to average during tare (zeroing)
const int TARE_READINGS = 20;

// ────────────────────────────────────────────────────────────
// PART CONFIGURATION
// ────────────────────────────────────────────────────────────
// Weight of a single part in grams
// Change this to match your specific part weight
float WEIGHT_PER_PART = 2.3;

// Weight of empty plastic packaging in grams
// Subtract this from total weight before counting
float PACKAGING_WEIGHT = 1.5;

// ────────────────────────────────────────────────────────────
// FILTERING SETTINGS
// ────────────────────────────────────────────────────────────
// Moving average window size
// Larger = smoother but slower response
// For 5kg load cell measuring small parts, use >= 10
const int MOVING_AVG_WINDOW = 15;

// Exponential smoothing factor (0.0 to 1.0)
// Lower = smoother, Higher = more responsive
// 0.1 recommended for 5kg cell with light parts
const float EMA_ALPHA = 0.1;

// Enable exponential smoothing in addition to moving average
const bool USE_EMA = true;

// Outlier rejection threshold in grams
// Readings that differ more than this from the average are rejected
const float OUTLIER_THRESHOLD = 5.0;

// ────────────────────────────────────────────────────────────
// STABILITY DETECTION
// ────────────────────────────────────────────────────────────
// Maximum allowed weight change (grams) to consider reading "stable"
// For 5kg cell with micro parts, keep this tight
const float STABILITY_THRESHOLD = 0.3;

// Number of consecutive stable readings required before confirming
const int STABILITY_COUNT = 8;

// ────────────────────────────────────────────────────────────
// TIMING
// ────────────────────────────────────────────────────────────
// Interval between sensor readings in milliseconds
// 50ms = 20 readings per second (good balance for HX711)
const unsigned long SAMPLE_INTERVAL_MS = 50;

// Interval between serial JSON output in milliseconds
// 500ms = 2 updates per second to Edge PC
const unsigned long OUTPUT_INTERVAL_MS = 500;

// ────────────────────────────────────────────────────────────
// ERROR DETECTION
// ────────────────────────────────────────────────────────────
// Maximum time (ms) to wait for HX711 to become ready
const unsigned long HX711_TIMEOUT_MS = 2000;

// Zero drift warning threshold in grams
// If tared weight drifts beyond this, warn the user
const float ZERO_DRIFT_WARN = 1.0;

// Maximum weight in grams (5kg = 5000g)
const float MAX_WEIGHT = 5000.0;

// Minimum valid weight to report (ignore noise below this)
const float MIN_VALID_WEIGHT = 0.5;

// ────────────────────────────────────────────────────────────
// SERIAL COMMUNICATION
// ────────────────────────────────────────────────────────────
// Baud rate for Serial communication
const long SERIAL_BAUD = 9600;

// Enable debug messages (set false for production)
const bool DEBUG_MODE = false;

#endif // CONFIG_H
