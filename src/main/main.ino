// ============================================================
// main.ino — Arduino Load Cell Weight Counting System
// ============================================================
// Project : Sistem Verifikasi Kuantitas Part Mikro dalam
//           Kemasan Plastik Menggunakan Density Map Estimation
//           dan Weight Counting Berbasis Edge Computing
//
// Hardware: Arduino Uno/Nano + 5kg Load Cell + HX711
// Pins   : DT → D3, SCK → D2
//
// This firmware:
//   1. Reads weight from HX711 with advanced filtering
//   2. Detects stable readings
//   3. Estimates part count
//   4. Sends JSON data over Serial to Edge PC
// ============================================================

#include "HX711.h"
#include "../config/config.h"

// ────────────────────────────────────────────────────────────
// GLOBAL OBJECTS & VARIABLES
// ────────────────────────────────────────────────────────────

// HX711 amplifier object
HX711 scale;

// Moving average buffer
float readings[MOVING_AVG_WINDOW];
int readingIndex = 0;
int validReadings = 0;  // how many readings we have collected so far

// Exponential Moving Average value
float emaValue = 0.0;
bool emaInitialized = false;

// Stability tracking
float lastStableWeight = 0.0;
int stableCounter = 0;
bool isStable = false;

// Timing (non-blocking)
unsigned long lastSampleTime = 0;
unsigned long lastOutputTime = 0;

// Current filtered weight
float filteredWeight = 0.0;

// Tare reference for zero drift detection
float tareReference = 0.0;

// Error flags
bool sensorConnected = true;
bool zeroDriftWarning = false;

// Calibration mode flag
bool calibrationMode = false;

// ────────────────────────────────────────────────────────────
// SETUP
// ────────────────────────────────────────────────────────────
void setup() {
  // Start serial communication
  Serial.begin(SERIAL_BAUD);

  // Wait for serial to be ready (important for some boards)
  while (!Serial) {
    ; // wait
  }

  Serial.println(F(""));
  Serial.println(F("========================================"));
  Serial.println(F("  Load Cell Weight Counting System"));
  Serial.println(F("  5kg Load Cell + HX711"));
  Serial.println(F("========================================"));
  Serial.println(F(""));

  // Initialize the HX711
  Serial.println(F("[INIT] Starting HX711..."));
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);

  // Check if HX711 is connected and responding
  if (!waitForHX711()) {
    sensorConnected = false;
    Serial.println(F("[ERROR] HX711 not detected! Check wiring."));
    Serial.println(F("[ERROR] DT → D3, SCK → D2"));
    sendErrorJSON("HX711_NOT_CONNECTED");
    return;
  }

  Serial.println(F("[OK] HX711 detected."));

  // Set the calibration factor
  scale.set_scale(CALIBRATION_FACTOR);
  Serial.print(F("[CONFIG] Calibration factor: "));
  Serial.println(CALIBRATION_FACTOR);

  // Tare the scale (set zero point)
  Serial.println(F("[INIT] Taring scale... Keep platform empty!"));
  scale.tare(TARE_READINGS);
  tareReference = 0.0;
  Serial.println(F("[OK] Tare complete."));

  // Initialize the moving average buffer with zeros
  for (int i = 0; i < MOVING_AVG_WINDOW; i++) {
    readings[i] = 0.0;
  }

  // Print part configuration
  Serial.print(F("[CONFIG] Weight per part: "));
  Serial.print(WEIGHT_PER_PART);
  Serial.println(F(" g"));

  Serial.print(F("[CONFIG] Packaging weight: "));
  Serial.print(PACKAGING_WEIGHT);
  Serial.println(F(" g"));

  Serial.println(F(""));
  Serial.println(F("[READY] System operational."));
  Serial.println(F("[TIP] Send 'C' to enter calibration mode."));
  Serial.println(F("[TIP] Send 'T' to tare (zero) the scale."));
  Serial.println(F(""));

  // Record start time
  lastSampleTime = millis();
  lastOutputTime = millis();
}

// ────────────────────────────────────────────────────────────
// MAIN LOOP
// ────────────────────────────────────────────────────────────
void loop() {
  // If sensor is not connected, keep trying to reconnect
  if (!sensorConnected) {
    handleDisconnectedSensor();
    return;
  }

  // Check for serial commands (calibration, tare, etc.)
  handleSerialCommands();

  // If in calibration mode, run calibration routine instead
  if (calibrationMode) {
    return;
  }

  // Get current time
  unsigned long currentTime = millis();

  // ── SAMPLE READING (non-blocking) ──
  if (currentTime - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = currentTime;

    // Take a raw reading from the HX711
    float rawWeight = readWeight();

    // Check for read errors
    if (rawWeight == -9999.0) {
      return; // sensor error, skip this cycle
    }

    // Apply outlier rejection
    if (isOutlier(rawWeight)) {
      if (DEBUG_MODE) {
        Serial.print(F("[DEBUG] Outlier rejected: "));
        Serial.println(rawWeight, 2);
      }
      return; // skip this reading
    }

    // Apply moving average filter
    float avgWeight = applyMovingAverage(rawWeight);

    // Apply exponential smoothing (if enabled)
    if (USE_EMA) {
      filteredWeight = applyEMA(avgWeight);
    } else {
      filteredWeight = avgWeight;
    }

    // Check stability
    checkStability(filteredWeight);

    // Check for zero drift
    checkZeroDrift(filteredWeight);
  }

  // ── SERIAL OUTPUT (non-blocking) ──
  if (currentTime - lastOutputTime >= OUTPUT_INTERVAL_MS) {
    lastOutputTime = currentTime;

    // Only output meaningful data when we have enough readings
    if (validReadings >= MOVING_AVG_WINDOW) {
      sendWeightJSON(filteredWeight, isStable);
    }
  }
}

// ============================================================
// HX711 COMMUNICATION FUNCTIONS
// ============================================================

// Wait for HX711 to become ready with a timeout
bool waitForHX711() {
  unsigned long startTime = millis();

  while (!scale.is_ready()) {
    // Check if we've exceeded the timeout
    if (millis() - startTime > HX711_TIMEOUT_MS) {
      return false;
    }
    delay(10);
  }

  return true;
}

// Read weight from HX711 with error handling
// Returns weight in grams, or -9999.0 on error
float readWeight() {
  // Check if HX711 is ready
  if (!scale.is_ready()) {
    if (DEBUG_MODE) {
      Serial.println(F("[DEBUG] HX711 not ready, skipping."));
    }
    return -9999.0;
  }

  // Get the weight reading in grams (based on calibration factor)
  float weight = scale.get_units(1);  // single reading, we filter ourselves

  // Sanity check: reject impossibly large values
  if (weight > MAX_WEIGHT || weight < -100.0) {
    if (DEBUG_MODE) {
      Serial.print(F("[DEBUG] Out of range: "));
      Serial.println(weight, 2);
    }
    return -9999.0;
  }

  return weight;
}

// ============================================================
// FILTERING FUNCTIONS
// ============================================================

// Check if a reading is an outlier compared to current average
bool isOutlier(float newReading) {
  // Don't reject during initial fill-up of the buffer
  if (validReadings < MOVING_AVG_WINDOW) {
    return false;
  }

  // Compare against current filtered value
  float difference = abs(newReading - filteredWeight);
  return (difference > OUTLIER_THRESHOLD);
}

// Add a reading to the moving average buffer and return the average
float applyMovingAverage(float newReading) {
  // Store reading in circular buffer
  readings[readingIndex] = newReading;
  readingIndex = (readingIndex + 1) % MOVING_AVG_WINDOW;

  // Track how many readings we've collected
  if (validReadings < MOVING_AVG_WINDOW) {
    validReadings++;
  }

  // Calculate average of all valid readings
  float sum = 0.0;
  int count = min(validReadings, MOVING_AVG_WINDOW);

  for (int i = 0; i < count; i++) {
    sum += readings[i];
  }

  return sum / count;
}

// Apply Exponential Moving Average (EMA) smoothing
float applyEMA(float newValue) {
  if (!emaInitialized) {
    // Initialize EMA with the first value
    emaValue = newValue;
    emaInitialized = true;
    return newValue;
  }

  // EMA formula: new_ema = alpha * new_value + (1 - alpha) * old_ema
  emaValue = (EMA_ALPHA * newValue) + ((1.0 - EMA_ALPHA) * emaValue);
  return emaValue;
}

// ============================================================
// STABILITY DETECTION
// ============================================================

// Check if the weight reading is stable
void checkStability(float currentWeight) {
  // Calculate how much the weight has changed since last check
  float delta = abs(currentWeight - lastStableWeight);

  if (delta < STABILITY_THRESHOLD) {
    // Weight is within threshold — count as stable
    stableCounter++;

    if (stableCounter >= STABILITY_COUNT) {
      isStable = true;
      // Cap the counter to prevent overflow
      stableCounter = STABILITY_COUNT;
    }
  } else {
    // Weight changed significantly — reset stability
    stableCounter = 0;
    isStable = false;
    lastStableWeight = currentWeight;
  }
}

// ============================================================
// ZERO DRIFT DETECTION
// ============================================================

// Check if the zero point has drifted
void checkZeroDrift(float currentWeight) {
  // Only check when nothing is on the scale (near zero)
  if (currentWeight > MIN_VALID_WEIGHT) {
    zeroDriftWarning = false;
    return;
  }

  // If weight should be zero but reads above threshold, warn
  if (abs(currentWeight) > ZERO_DRIFT_WARN) {
    if (!zeroDriftWarning) {
      zeroDriftWarning = true;
      if (DEBUG_MODE) {
        Serial.println(F("[WARN] Zero drift detected. Consider re-taring."));
      }
    }
  } else {
    zeroDriftWarning = false;
  }
}

// ============================================================
// PART COUNT ESTIMATION
// ============================================================

// Estimate the number of parts based on weight
float estimatePartCount(float totalWeight) {
  // Subtract packaging weight
  float netWeight = totalWeight - PACKAGING_WEIGHT;

  // Don't return negative counts
  if (netWeight < 0.0) {
    netWeight = 0.0;
  }

  // Calculate count (keep as float for precision)
  float count = netWeight / WEIGHT_PER_PART;

  return count;
}

// Get the nearest integer count
int estimatePartCountRounded(float totalWeight) {
  float exactCount = estimatePartCount(totalWeight);
  return (int)(exactCount + 0.5);  // round to nearest integer
}

// ============================================================
// SERIAL OUTPUT — JSON FORMAT
// ============================================================

// Send weight data as JSON to the Edge PC
void sendWeightJSON(float weight, bool stable) {
  // Calculate part count
  float exactCount = estimatePartCount(weight);
  int roundedCount = estimatePartCountRounded(weight);

  // Net weight (after subtracting packaging)
  float netWeight = weight - PACKAGING_WEIGHT;
  if (netWeight < 0.0) netWeight = 0.0;

  // Build JSON string manually (no ArduinoJson library needed)
  Serial.print(F("{"));

  // Gross weight (total including packaging)
  Serial.print(F("\"weight_gross\":"));
  Serial.print(weight, 2);

  // Net weight (parts only)
  Serial.print(F(",\"weight_net\":"));
  Serial.print(netWeight, 2);

  // Exact count estimate (float)
  Serial.print(F(",\"count_estimate\":"));
  Serial.print(exactCount, 2);

  // Rounded count estimate (integer)
  Serial.print(F(",\"count_rounded\":"));
  Serial.print(roundedCount);

  // Stability flag
  Serial.print(F(",\"stable\":"));
  Serial.print(stable ? F("true") : F("false"));

  // Zero drift warning
  Serial.print(F(",\"zero_drift\":"));
  Serial.print(zeroDriftWarning ? F("true") : F("false"));

  // Weight per part (for reference)
  Serial.print(F(",\"weight_per_part\":"));
  Serial.print(WEIGHT_PER_PART, 2);

  Serial.println(F("}"));
}

// Send an error message as JSON
void sendErrorJSON(const char* errorType) {
  Serial.print(F("{\"error\":\""));
  Serial.print(errorType);
  Serial.print(F("\",\"stable\":false,\"weight_gross\":0.00"));
  Serial.println(F("}"));
}

// ============================================================
// SERIAL COMMAND HANDLING
// ============================================================

// Process incoming serial commands from Edge PC or user
void handleSerialCommands() {
  if (!Serial.available()) {
    return;
  }

  char command = Serial.read();

  switch (command) {
    case 'C':
    case 'c':
      // Enter calibration mode
      startCalibration();
      break;

    case 'T':
    case 't':
      // Tare (zero) the scale
      performTare();
      break;

    case 'R':
    case 'r':
      // Reset the system
      resetSystem();
      break;

    case 'D':
    case 'd':
      // Toggle debug mode
      toggleDebug();
      break;

    case 'S':
    case 's':
      // Print current status
      printStatus();
      break;
  }

  // Clear any remaining characters in the buffer
  while (Serial.available()) {
    Serial.read();
  }
}

// ============================================================
// CALIBRATION SYSTEM
// ============================================================

// Interactive calibration procedure
void startCalibration() {
  calibrationMode = true;

  Serial.println(F(""));
  Serial.println(F("========================================"));
  Serial.println(F("  CALIBRATION MODE"));
  Serial.println(F("  5kg Load Cell Calibration"));
  Serial.println(F("========================================"));
  Serial.println(F(""));
  Serial.println(F("Commands:"));
  Serial.println(F("  1 — Tare (zero) the scale"));
  Serial.println(F("  2 — Calibrate with known weight"));
  Serial.println(F("  3 — Show current raw value"));
  Serial.println(F("  4 — Set calibration factor manually"));
  Serial.println(F("  5 — Test current calibration"));
  Serial.println(F("  6 — Set weight per part"));
  Serial.println(F("  7 — Set packaging weight"));
  Serial.println(F("  0 — Exit calibration mode"));
  Serial.println(F(""));

  // Calibration loop
  while (calibrationMode) {
    if (Serial.available()) {
      char choice = Serial.read();

      // Clear buffer
      while (Serial.available()) Serial.read();

      switch (choice) {
        case '1':
          calibrateTare();
          break;
        case '2':
          calibrateWithKnownWeight();
          break;
        case '3':
          showRawValue();
          break;
        case '4':
          setCalibrationFactorManual();
          break;
        case '5':
          testCalibration();
          break;
        case '6':
          setWeightPerPart();
          break;
        case '7':
          setPackagingWeight();
          break;
        case '0':
          calibrationMode = false;
          Serial.println(F("[CAL] Exiting calibration mode."));
          Serial.println(F("[TIP] Update config.h with new values!"));
          Serial.println(F(""));
          break;
      }
    }
    delay(10);  // Small delay in calibration loop
  }
}

// Step 1: Tare in calibration mode
void calibrateTare() {
  Serial.println(F(""));
  Serial.println(F("[CAL] Remove all items from the scale."));
  Serial.println(F("[CAL] Press any key when ready..."));
  waitForSerialInput();

  Serial.println(F("[CAL] Taring..."));
  scale.set_scale();  // Reset to default
  scale.tare(TARE_READINGS);
  Serial.println(F("[CAL] Tare complete. Zero point set."));
  Serial.println(F(""));
}

// Step 2: Calibrate using a known weight
void calibrateWithKnownWeight() {
  Serial.println(F(""));
  Serial.println(F("[CAL] ── Known Weight Calibration ──"));
  Serial.println(F("[CAL] Step 1: Make sure the scale is tared (option 1)"));
  Serial.println(F("[CAL] Step 2: Place a known weight on the scale"));
  Serial.println(F("[CAL] Recommended: Use 100g or 200g calibration weight"));
  Serial.println(F("[CAL] Press any key when weight is placed..."));
  waitForSerialInput();

  // Get raw reading
  long rawValue = scale.get_units(20);  // Average of 20 readings

  Serial.print(F("[CAL] Raw averaged value: "));
  Serial.println(rawValue);

  Serial.println(F("[CAL] Enter the known weight in grams (e.g., 100):"));
  float knownWeight = readSerialFloat();

  if (knownWeight > 0) {
    // Calculate calibration factor
    CALIBRATION_FACTOR = (float)rawValue / knownWeight;
    scale.set_scale(CALIBRATION_FACTOR);

    Serial.println(F(""));
    Serial.println(F("[CAL] ✓ Calibration complete!"));
    Serial.print(F("[CAL] New calibration factor: "));
    Serial.println(CALIBRATION_FACTOR, 4);
    Serial.println(F(""));
    Serial.println(F("[CAL] ⚠ UPDATE config.h:"));
    Serial.print(F("[CAL]   float CALIBRATION_FACTOR = "));
    Serial.print(CALIBRATION_FACTOR, 4);
    Serial.println(F(";"));
    Serial.println(F(""));
  } else {
    Serial.println(F("[CAL] Invalid weight. Calibration cancelled."));
  }
}

// Step 3: Show raw HX711 value
void showRawValue() {
  Serial.println(F(""));
  Serial.println(F("[CAL] Reading raw values (10 samples)..."));

  for (int i = 0; i < 10; i++) {
    if (scale.is_ready()) {
      long rawReading = scale.read();
      float calibratedReading = scale.get_units(1);

      Serial.print(F("  Raw: "));
      Serial.print(rawReading);
      Serial.print(F("  |  Calibrated: "));
      Serial.print(calibratedReading, 2);
      Serial.println(F(" g"));
    }
    delay(200);
  }
  Serial.println(F(""));
}

// Step 4: Manually set calibration factor
void setCalibrationFactorManual() {
  Serial.println(F(""));
  Serial.print(F("[CAL] Current factor: "));
  Serial.println(CALIBRATION_FACTOR, 4);
  Serial.println(F("[CAL] Enter new calibration factor:"));

  float newFactor = readSerialFloat();

  if (newFactor != 0) {
    CALIBRATION_FACTOR = newFactor;
    scale.set_scale(CALIBRATION_FACTOR);

    Serial.print(F("[CAL] Factor updated to: "));
    Serial.println(CALIBRATION_FACTOR, 4);
    Serial.println(F("[CAL] Remember to update config.h!"));
  } else {
    Serial.println(F("[CAL] Invalid value. No change."));
  }
  Serial.println(F(""));
}

// Step 5: Test current calibration
void testCalibration() {
  Serial.println(F(""));
  Serial.println(F("[CAL] Testing calibration (10 readings)..."));
  Serial.println(F("[CAL] Place a known weight on the scale."));

  for (int i = 0; i < 10; i++) {
    if (scale.is_ready()) {
      float weight = scale.get_units(5);  // Average of 5
      Serial.print(F("  Reading "));
      Serial.print(i + 1);
      Serial.print(F(": "));
      Serial.print(weight, 2);
      Serial.println(F(" g"));
    }
    delay(300);
  }
  Serial.println(F(""));
}

// Step 6: Set weight per part
void setWeightPerPart() {
  Serial.println(F(""));
  Serial.print(F("[CAL] Current weight per part: "));
  Serial.print(WEIGHT_PER_PART, 2);
  Serial.println(F(" g"));
  Serial.println(F("[CAL] Enter new weight per part in grams:"));

  float newWeight = readSerialFloat();

  if (newWeight > 0) {
    WEIGHT_PER_PART = newWeight;
    Serial.print(F("[CAL] Weight per part set to: "));
    Serial.print(WEIGHT_PER_PART, 2);
    Serial.println(F(" g"));
    Serial.println(F("[CAL] Remember to update config.h!"));
  } else {
    Serial.println(F("[CAL] Invalid value. No change."));
  }
  Serial.println(F(""));
}

// Step 7: Set packaging weight
void setPackagingWeight() {
  Serial.println(F(""));
  Serial.print(F("[CAL] Current packaging weight: "));
  Serial.print(PACKAGING_WEIGHT, 2);
  Serial.println(F(" g"));
  Serial.println(F("[CAL] Enter new packaging weight in grams:"));

  float newWeight = readSerialFloat();

  if (newWeight >= 0) {
    PACKAGING_WEIGHT = newWeight;
    Serial.print(F("[CAL] Packaging weight set to: "));
    Serial.print(PACKAGING_WEIGHT, 2);
    Serial.println(F(" g"));
    Serial.println(F("[CAL] Remember to update config.h!"));
  } else {
    Serial.println(F("[CAL] Invalid value. No change."));
  }
  Serial.println(F(""));
}

// ============================================================
// UTILITY FUNCTIONS
// ============================================================

// Perform tare (zero) the scale
void performTare() {
  Serial.println(F("[TARE] Zeroing scale..."));
  scale.tare(TARE_READINGS);
  tareReference = 0.0;

  // Reset filters
  for (int i = 0; i < MOVING_AVG_WINDOW; i++) {
    readings[i] = 0.0;
  }
  readingIndex = 0;
  validReadings = 0;
  emaValue = 0.0;
  emaInitialized = false;
  stableCounter = 0;
  isStable = false;

  Serial.println(F("[TARE] Done. Scale zeroed."));
}

// Reset the entire system
void resetSystem() {
  Serial.println(F("[RESET] Resetting system..."));
  performTare();
  scale.set_scale(CALIBRATION_FACTOR);
  Serial.println(F("[RESET] System reset complete."));
}

// Toggle debug messages
void toggleDebug() {
  // Note: DEBUG_MODE is const, so we use a workaround here
  // In production, modify config.h directly
  Serial.println(F("[DEBUG] Toggle debug via config.h (DEBUG_MODE)"));
}

// Print current system status
void printStatus() {
  Serial.println(F(""));
  Serial.println(F("── System Status ──"));
  Serial.print(F("  Calibration factor: "));
  Serial.println(CALIBRATION_FACTOR, 4);
  Serial.print(F("  Weight per part: "));
  Serial.print(WEIGHT_PER_PART, 2);
  Serial.println(F(" g"));
  Serial.print(F("  Packaging weight: "));
  Serial.print(PACKAGING_WEIGHT, 2);
  Serial.println(F(" g"));
  Serial.print(F("  Current weight: "));
  Serial.print(filteredWeight, 2);
  Serial.println(F(" g"));
  Serial.print(F("  Stable: "));
  Serial.println(isStable ? F("YES") : F("NO"));
  Serial.print(F("  Stability counter: "));
  Serial.print(stableCounter);
  Serial.print(F("/"));
  Serial.println(STABILITY_COUNT);
  Serial.print(F("  Valid readings: "));
  Serial.println(validReadings);
  Serial.print(F("  Zero drift warning: "));
  Serial.println(zeroDriftWarning ? F("YES") : F("NO"));
  Serial.println(F(""));
}

// Handle disconnected sensor — attempt reconnection
void handleDisconnectedSensor() {
  Serial.println(F("[ERROR] Sensor disconnected. Retrying..."));
  sendErrorJSON("HX711_NOT_CONNECTED");

  delay(2000);  // Wait before retry

  if (waitForHX711()) {
    sensorConnected = true;
    Serial.println(F("[OK] HX711 reconnected!"));
    resetSystem();
  }
}

// Wait for any serial input (blocking, used in calibration only)
void waitForSerialInput() {
  while (!Serial.available()) {
    delay(10);
  }
  // Clear the input
  while (Serial.available()) {
    Serial.read();
  }
}

// Read a float value from serial input (blocking, used in calibration only)
float readSerialFloat() {
  // Wait for input
  while (!Serial.available()) {
    delay(10);
  }

  // Read the entire line
  String input = "";
  unsigned long startTime = millis();

  // Wait up to 5 seconds for complete input
  while (millis() - startTime < 5000) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') {
        if (input.length() > 0) {
          break;
        }
      } else {
        input += c;
      }
    }
    delay(5);
  }

  // Convert to float
  float value = input.toFloat();
  return value;
}
