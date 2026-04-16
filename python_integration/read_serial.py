"""
============================================================
read_serial.py — Arduino Load Cell Serial Reader + AI Fusion
============================================================
Project: Sistem Verifikasi Kuantitas Part Mikro dalam
         Kemasan Plastik Menggunakan Density Map Estimation
         dan Weight Counting Berbasis Edge Computing

This script:
  1. Reads JSON weight data from Arduino via Serial
  2. Optionally receives AI density map prediction
  3. Fuses both estimates to make OK/NG decision
  4. Logs all data with timestamps

Usage:
  python read_serial.py                    # Default COM3
  python read_serial.py --port COM5        # Custom port
  python read_serial.py --port /dev/ttyUSB0  # Linux
============================================================
"""

import serial
import json
import time
import argparse
import logging
import csv
import os
from datetime import datetime

# ────────────────────────────────────────────────────────────
# CONFIGURATION
# ────────────────────────────────────────────────────────────

# Default serial port settings
DEFAULT_PORT = "COM3"
DEFAULT_BAUD = 9600
SERIAL_TIMEOUT = 2  # seconds

# Decision logic settings
TOLERANCE_COUNT = 3       # Maximum allowed difference between AI and weight count
TOLERANCE_PERCENT = 0.03  # Maximum allowed percentage error (3%)

# Expected count (set per batch, or received from system)
EXPECTED_COUNT = None  # Set to integer if known, else None

# Logging configuration
LOG_DIR = "logs"
LOG_FILE = os.path.join(LOG_DIR, f"session_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv")

# ────────────────────────────────────────────────────────────
# ARGUMENT PARSER
# ────────────────────────────────────────────────────────────

def parse_arguments():
    """Parse command-line arguments for serial port configuration."""
    parser = argparse.ArgumentParser(
        description="Arduino Load Cell Serial Reader + AI Fusion"
    )
    parser.add_argument(
        "--port", type=str, default=DEFAULT_PORT,
        help=f"Serial port (default: {DEFAULT_PORT})"
    )
    parser.add_argument(
        "--baud", type=int, default=DEFAULT_BAUD,
        help=f"Baud rate (default: {DEFAULT_BAUD})"
    )
    parser.add_argument(
        "--tolerance", type=int, default=TOLERANCE_COUNT,
        help=f"Maximum count difference for OK decision (default: {TOLERANCE_COUNT})"
    )
    parser.add_argument(
        "--expected", type=int, default=None,
        help="Expected part count (optional, for absolute verification)"
    )
    parser.add_argument(
        "--log", action="store_true", default=True,
        help="Enable CSV logging (default: enabled)"
    )
    parser.add_argument(
        "--verbose", action="store_true", default=False,
        help="Enable verbose output"
    )
    return parser.parse_args()


# ────────────────────────────────────────────────────────────
# LOGGING SETUP
# ────────────────────────────────────────────────────────────

def setup_logging(verbose=False):
    """Configure the logging system."""
    level = logging.DEBUG if verbose else logging.INFO
    logging.basicConfig(
        level=level,
        format="%(asctime)s [%(levelname)s] %(message)s",
        datefmt="%H:%M:%S"
    )
    return logging.getLogger("LoadCellReader")


def init_csv_log(log_file):
    """Initialize CSV log file with headers."""
    os.makedirs(os.path.dirname(log_file), exist_ok=True)

    with open(log_file, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow([
            "timestamp",
            "weight_gross",
            "weight_net",
            "count_weight",
            "count_rounded",
            "stable",
            "count_ai",
            "decision",
            "count_difference",
            "zero_drift"
        ])
    return log_file


def log_to_csv(log_file, data):
    """Append a data row to the CSV log."""
    with open(log_file, "a", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(data)


# ────────────────────────────────────────────────────────────
# SERIAL CONNECTION
# ────────────────────────────────────────────────────────────

def connect_serial(port, baud, logger):
    """
    Establish serial connection to Arduino.
    Retries connection on failure.
    """
    while True:
        try:
            logger.info(f"Connecting to {port} at {baud} baud...")
            ser = serial.Serial(
                port=port,
                baudrate=baud,
                timeout=SERIAL_TIMEOUT,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE
            )
            # Wait for Arduino to reset after serial connection
            time.sleep(2)

            # Flush any startup messages
            ser.reset_input_buffer()

            logger.info(f"Connected to {port} successfully!")
            return ser

        except serial.SerialException as e:
            logger.error(f"Connection failed: {e}")
            logger.info("Retrying in 3 seconds...")
            time.sleep(3)


def read_json_line(ser, logger):
    """
    Read one line from serial and parse as JSON.
    Returns parsed dict or None if invalid.
    """
    try:
        # Read a line from serial
        raw_line = ser.readline().decode("utf-8", errors="ignore").strip()

        # Skip empty lines and non-JSON lines
        if not raw_line or not raw_line.startswith("{"):
            if raw_line:
                logger.debug(f"Non-JSON: {raw_line}")
            return None

        # Parse JSON
        data = json.loads(raw_line)
        return data

    except json.JSONDecodeError as e:
        logger.warning(f"Invalid JSON: {e}")
        return None
    except serial.SerialException as e:
        logger.error(f"Serial read error: {e}")
        return None
    except Exception as e:
        logger.error(f"Unexpected error: {e}")
        return None


# ────────────────────────────────────────────────────────────
# AI INTEGRATION (DENSITY MAP ESTIMATION)
# ────────────────────────────────────────────────────────────

def get_ai_prediction():
    """
    Get the latest AI density map prediction count.

    In production, this function should:
    1. Read from a shared file/database updated by the DME model
    2. Or receive via API/socket from the vision system
    3. Or read from a message queue (Redis, RabbitMQ, etc.)

    For now, it returns None (no AI prediction available).
    Replace this with your actual DME model integration.
    """
    # ── OPTION 1: Read from shared file ──
    # This is the simplest integration method.
    # The DME model writes its prediction to a JSON file,
    # and this script reads it.
    prediction_file = "ai_prediction.json"

    try:
        if os.path.exists(prediction_file):
            # Only read if file was updated recently (within 10 seconds)
            file_age = time.time() - os.path.getmtime(prediction_file)
            if file_age < 10:
                with open(prediction_file, "r") as f:
                    prediction = json.load(f)
                return prediction.get("count", None)
    except (json.JSONDecodeError, IOError):
        pass

    return None

    # ── OPTION 2: Read from API (uncomment to use) ──
    # import requests
    # try:
    #     response = requests.get("http://localhost:5000/api/prediction", timeout=1)
    #     if response.status_code == 200:
    #         return response.json().get("count", None)
    # except requests.RequestException:
    #     pass
    # return None

    # ── OPTION 3: Read from Redis (uncomment to use) ──
    # import redis
    # try:
    #     r = redis.Redis(host='localhost', port=6379, db=0)
    #     count = r.get('dme_count')
    #     if count:
    #         return float(count)
    # except redis.RedisError:
    #     pass
    # return None


# ────────────────────────────────────────────────────────────
# DECISION LOGIC (SENSOR FUSION)
# ────────────────────────────────────────────────────────────

def make_decision(weight_count, ai_count, expected_count=None, tolerance=3):
    """
    Determine if the part count is OK or NG (Not Good).

    Decision matrix:
    ┌────────────────────────────────────────────────────────┐
    │ Scenario              │ Logic                          │
    ├────────────────────────────────────────────────────────┤
    │ Both sources agree    │ |AI - Weight| < tolerance → OK │
    │ Both sources disagree │ |AI - Weight| >= tolerance → NG│
    │ Only weight available │ Check against expected → OK/NG │
    │ No data available     │ UNKNOWN                        │
    └────────────────────────────────────────────────────────┘

    Args:
        weight_count: Integer count from weight-based estimation
        ai_count: Float/int count from density map estimation (or None)
        expected_count: Expected count per batch (or None)
        tolerance: Maximum allowed difference between counts

    Returns:
        dict with decision, confidence, and reasoning
    """
    result = {
        "decision": "UNKNOWN",
        "confidence": 0.0,
        "weight_count": weight_count,
        "ai_count": ai_count,
        "expected_count": expected_count,
        "difference": None,
        "reason": ""
    }

    # ── Case 1: Both AI and Weight data available ──
    if ai_count is not None and weight_count is not None:
        difference = abs(ai_count - weight_count)
        result["difference"] = round(difference, 2)

        if difference < tolerance:
            result["decision"] = "OK"
            result["confidence"] = max(0, 1.0 - (difference / tolerance))
            result["reason"] = f"AI ({ai_count}) and Weight ({weight_count}) agree within tolerance ({tolerance})"
        else:
            result["decision"] = "NG"
            result["confidence"] = min(1.0, difference / tolerance)
            result["reason"] = f"AI ({ai_count}) and Weight ({weight_count}) disagree by {difference:.1f} (tolerance: {tolerance})"

        # Cross-check with expected count if available
        if expected_count is not None:
            weight_error = abs(weight_count - expected_count)
            ai_error = abs(ai_count - expected_count)

            if weight_error <= tolerance and ai_error <= tolerance:
                result["decision"] = "OK"
                result["reason"] += f" | Both match expected ({expected_count})"
            elif weight_error > tolerance and ai_error > tolerance:
                result["decision"] = "NG"
                result["reason"] += f" | Both differ from expected ({expected_count})"

    # ── Case 2: Only weight data available ──
    elif weight_count is not None:
        if expected_count is not None:
            difference = abs(weight_count - expected_count)
            result["difference"] = round(difference, 2)

            if difference <= tolerance:
                result["decision"] = "OK"
                result["confidence"] = max(0, 1.0 - (difference / tolerance))
                result["reason"] = f"Weight count ({weight_count}) matches expected ({expected_count})"
            else:
                result["decision"] = "NG"
                result["confidence"] = min(1.0, difference / tolerance)
                result["reason"] = f"Weight count ({weight_count}) differs from expected ({expected_count}) by {difference}"
        else:
            result["decision"] = "WEIGHT_ONLY"
            result["confidence"] = 0.5
            result["reason"] = f"Only weight data available. Count: {weight_count}. No AI or expected for comparison."

    # ── Case 3: No valid data ──
    else:
        result["decision"] = "UNKNOWN"
        result["reason"] = "No valid count data available"

    return result


# ────────────────────────────────────────────────────────────
# DISPLAY
# ────────────────────────────────────────────────────────────

def display_reading(data, decision_result, logger):
    """Display the current reading in a formatted way."""
    # Clear line and print
    timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]

    weight_gross = data.get("weight_gross", 0)
    weight_net = data.get("weight_net", 0)
    count_est = data.get("count_estimate", 0)
    count_round = data.get("count_rounded", 0)
    stable = data.get("stable", False)

    # Status indicator
    stable_icon = "●" if stable else "○"
    decision = decision_result.get("decision", "---")

    # Color decision
    if decision == "OK":
        decision_display = f"✓ OK"
    elif decision == "NG":
        decision_display = f"✗ NG"
    else:
        decision_display = f"? {decision}"

    print(f"[{timestamp}] {stable_icon} "
          f"Gross: {weight_gross:8.2f}g | "
          f"Net: {weight_net:8.2f}g | "
          f"Count: {count_round:4d} ({count_est:.2f}) | "
          f"Decision: {decision_display}")


# ────────────────────────────────────────────────────────────
# MAIN EXECUTION
# ────────────────────────────────────────────────────────────

def main():
    """Main execution loop."""
    args = parse_arguments()
    logger = setup_logging(args.verbose)

    # Print banner
    print("")
    print("=" * 60)
    print("  Load Cell Reader + AI Fusion System")
    print("  Sistem Verifikasi Kuantitas Part Mikro")
    print("=" * 60)
    print("")

    # Initialize CSV logging
    log_file = None
    if args.log:
        log_file = init_csv_log(LOG_FILE)
        logger.info(f"Logging to: {log_file}")

    # Connect to Arduino
    ser = connect_serial(args.port, args.baud, logger)

    # Set expected count
    expected = args.expected

    # Reading counter
    reading_count = 0
    stable_readings = 0

    print("")
    print("─" * 60)
    print("  Reading data... (Press Ctrl+C to stop)")
    print("─" * 60)
    print("")

    try:
        while True:
            # Read JSON from Arduino
            data = read_json_line(ser, logger)

            if data is None:
                continue

            # Check for errors from Arduino
            if "error" in data:
                logger.error(f"Arduino error: {data['error']}")
                continue

            reading_count += 1

            # Extract weight count
            weight_count = data.get("count_rounded", None)
            is_stable = data.get("stable", False)

            if is_stable:
                stable_readings += 1

            # Get AI prediction (from density map model)
            ai_count = get_ai_prediction()

            # Make fusion decision
            decision_result = make_decision(
                weight_count=weight_count,
                ai_count=ai_count,
                expected_count=expected,
                tolerance=args.tolerance
            )

            # Display the reading
            display_reading(data, decision_result, logger)

            # Log to CSV
            if log_file:
                log_to_csv(log_file, [
                    datetime.now().isoformat(),
                    data.get("weight_gross", 0),
                    data.get("weight_net", 0),
                    data.get("count_estimate", 0),
                    data.get("count_rounded", 0),
                    is_stable,
                    ai_count if ai_count else "",
                    decision_result["decision"],
                    decision_result.get("difference", ""),
                    data.get("zero_drift", False),
                ])

            # When stable, print decision summary
            if is_stable and stable_readings % 5 == 0:
                logger.info(f"Decision: {decision_result['decision']} "
                          f"| Reason: {decision_result['reason']}")

    except KeyboardInterrupt:
        print("")
        print("─" * 60)
        print(f"  Session ended. Total readings: {reading_count}")
        print(f"  Stable readings: {stable_readings}")
        if log_file:
            print(f"  Log saved to: {log_file}")
        print("─" * 60)

    finally:
        ser.close()
        logger.info("Serial connection closed.")


# ────────────────────────────────────────────────────────────
# ENTRY POINT
# ────────────────────────────────────────────────────────────

if __name__ == "__main__":
    main()
