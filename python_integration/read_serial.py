# ============================================================
# read_serial.py — Arduino + AI Fusion (Clean Version)
# ============================================================

import serial
import json
import time
import argparse
import logging
from datetime import datetime

# 🔥 IMPORT YOUR AI FUNCTION
# Make sure predict.py has: def predict_image(image_path)
from predict import predict_image


# ==============================
# CONFIG
# ==============================
DEFAULT_PORT = "COM3"
DEFAULT_BAUD = 9600
SERIAL_TIMEOUT = 2

TOLERANCE_COUNT = 3
IMAGE_PATH = "test.jpg"


# ==============================
# ARGUMENT PARSER
# ==============================
def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", type=str, default=DEFAULT_PORT)
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    parser.add_argument("--tolerance", type=int, default=TOLERANCE_COUNT)
    return parser.parse_args()


# ==============================
# LOGGING
# ==============================
def setup_logging():
    logging.basicConfig(
        level=logging.INFO,
        format="%(asctime)s [%(levelname)s] %(message)s",
        datefmt="%H:%M:%S"
    )
    return logging.getLogger("FusionSystem")


# ==============================
# SERIAL
# ==============================
def connect_serial(port, baud, logger):
    while True:
        try:
            logger.info(f"Connecting to {port}...")
            ser = serial.Serial(port, baud, timeout=SERIAL_TIMEOUT)
            time.sleep(2)
            ser.reset_input_buffer()
            logger.info("Connected!")
            return ser
        except Exception as e:
            logger.error(f"Failed: {e}")
            time.sleep(2)


def read_json_line(ser):
    try:
        line = ser.readline().decode().strip()
        if line.startswith("{"):
            return json.loads(line)
    except:
        return None
    return None


# ==============================
# AI PREDICTION
# ==============================
def get_ai_prediction(logger):
    try:
        count = predict_image(IMAGE_PATH)
        return count
    except Exception as e:
        logger.warning(f"AI error: {e}")
        return None


# ==============================
# DECISION LOGIC
# ==============================
def make_decision(weight_count, ai_count, tolerance):
    if ai_count is None:
        return "WEIGHT_ONLY"

    diff = abs(ai_count - weight_count)

    if diff < tolerance:
        return "OK"
    else:
        return "NG"


# ==============================
# DISPLAY
# ==============================
def display(data, ai_count, decision):
    print(
        f"Weight: {data.get('weight_net', 0):.2f}g | "
        f"Count(W): {data.get('count_rounded', 0)} | "
        f"Count(AI): {ai_count} | "
        f"Decision: {decision}"
    )


# ==============================
# MAIN
# ==============================
def main():
    args = parse_arguments()
    logger = setup_logging()

    ser = connect_serial(args.port, args.baud, logger)

    print("\nRunning system...\n")

    while True:
        data = read_json_line(ser)

        if data is None:
            continue

        if "error" in data:
            logger.error(data["error"])
            continue

        weight_count = data.get("count_rounded", 0)
        stable = data.get("stable", False)

        # Only process when stable
        if not stable:
            continue

        # AI prediction
        ai_count = get_ai_prediction(logger)

        # Decision
        decision = make_decision(weight_count, ai_count, args.tolerance)

        # Display
        display(data, ai_count, decision)


if __name__ == "__main__":
    main()