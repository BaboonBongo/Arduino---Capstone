# Integration with AI — Density Map Estimation

## 🏗️ System Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      EDGE PC (Python)                           │
│                                                                 │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────────┐  │
│  │   Camera      │    │   Arduino     │    │   Fusion Engine  │  │
│  │   + DME Model │    │   Serial Port │    │   (Decision)     │  │
│  │              │    │              │    │                  │  │
│  │  AI Count ───┼───▶│              │    │  ┌────────────┐  │  │
│  │  (PyTorch)   │    │ Weight Count ─┼───▶│  │  Compare   │  │  │
│  └──────────────┘    └──────────────┘    │  │  AI vs     │  │  │
│                                          │  │  Weight    │  │  │
│                                          │  └─────┬──────┘  │  │
│                                          │        │         │  │
│                                          │   OK / NG        │  │
│                                          └──────────────────┘  │
└─────────────────────────────────────────────────────────────────┘

Hardware Layer:
┌───────────────┐         ┌───────────────┐
│   USB Camera  │         │   Arduino     │
│   (1080p)     │         │   + HX711     │
│   + Lighting  │         │   + Load Cell │
└───────┬───────┘         └───────┬───────┘
        │ USB                     │ USB/Serial
        └─────────┬───────────────┘
                  │
           ┌──────┴──────┐
           │   Edge PC   │
           │ (Jetson/PC) │
           └─────────────┘
```

---

## 📡 Data Flow

### Arduino → Edge PC (Serial JSON)

The Arduino sends weight data as JSON over serial at **2 Hz** (every 500ms):

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
| `weight_gross` | float | Total weight including packaging (grams) |
| `weight_net` | float | Weight of parts only (grams) |
| `count_estimate` | float | Exact part count estimate |
| `count_rounded` | int | Rounded part count |
| `stable` | bool | True when reading is stable |
| `zero_drift` | bool | True if zero point has drifted |
| `weight_per_part` | float | Configured weight per part |

### Camera → DME Model → Prediction

The density map estimation model processes camera frames:

```python
# DME model output
{
    "count": 65,           # Estimated count from density map
    "confidence": 0.92,    # Model confidence (0-1)
    "density_map": [...],  # 2D density map (optional)
    "timestamp": "..."     # When prediction was made
}
```

### Fusion Decision

Both sources are combined:

```python
# If |AI_count - Weight_count| < tolerance → OK
# If |AI_count - Weight_count| >= tolerance → NG

Example:
  AI count     = 65
  Weight count = 66
  Tolerance    = 3
  Difference   = |65 - 66| = 1
  Result       = 1 < 3 → ✓ OK
```

---

## 🔗 Integration Methods

### Method 1: File-Based (Simplest)

The DME model writes predictions to a JSON file. The serial reader reads it.

**DME Model writes:**
```python
# In your DME inference script
import json

prediction = {
    "count": estimated_count,
    "confidence": confidence_score,
    "timestamp": datetime.now().isoformat()
}

with open("ai_prediction.json", "w") as f:
    json.dump(prediction, f)
```

**Serial reader reads:** (already implemented in `read_serial.py`)
```python
def get_ai_prediction():
    if os.path.exists("ai_prediction.json"):
        with open("ai_prediction.json", "r") as f:
            prediction = json.load(f)
        return prediction.get("count", None)
    return None
```

### Method 2: API-Based (Recommended for Production)

The DME model runs as a Flask API. The serial reader queries it.

**DME API Server:**
```python
# dme_server.py
from flask import Flask, jsonify
import torch

app = Flask(__name__)
model = load_dme_model()  # Your DME model

@app.route("/api/prediction", methods=["GET"])
def get_prediction():
    frame = capture_frame()
    density_map = model(frame)
    count = density_map.sum().item()

    return jsonify({
        "count": round(count),
        "confidence": calculate_confidence(density_map),
        "timestamp": datetime.now().isoformat()
    })

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)
```

**Serial reader queries:**
```python
import requests

def get_ai_prediction():
    try:
        response = requests.get("http://localhost:5000/api/prediction", timeout=1)
        if response.status_code == 200:
            return response.json().get("count", None)
    except requests.RequestException:
        pass
    return None
```

### Method 3: Shared Queue (Most Robust)

Use Redis or a similar message queue for real-time communication.

```python
# DME model publishes
import redis
r = redis.Redis()
r.set("dme_count", str(count))
r.set("dme_timestamp", datetime.now().isoformat())

# Serial reader subscribes
count = float(r.get("dme_count"))
```

---

## 🧠 Fusion Decision Logic

### Basic Decision

```python
def make_decision(weight_count, ai_count, tolerance=3):
    if ai_count is None:
        return "WEIGHT_ONLY"  # Only weight data available

    difference = abs(ai_count - weight_count)

    if difference < tolerance:
        return "OK"   # Both sources agree
    else:
        return "NG"   # Sources disagree — investigate
```

### Advanced Decision with Confidence Weighting

```python
def advanced_decision(weight_count, ai_count, ai_confidence,
                      expected_count=None, tolerance=3):
    """
    Weighted fusion decision considering AI confidence.

    When AI confidence is high:
      - Trust AI more, weight confirms
    When AI confidence is low:
      - Trust weight more, AI is supplementary
    """
    if ai_count is None:
        # Weight-only mode
        if expected_count:
            return "OK" if abs(weight_count - expected_count) <= tolerance else "NG"
        return "WEIGHT_ONLY"

    # Weighted average based on AI confidence
    if ai_confidence > 0.8:
        # High confidence: 60% AI, 40% weight
        fused_count = 0.6 * ai_count + 0.4 * weight_count
    elif ai_confidence > 0.5:
        # Medium confidence: equal weight
        fused_count = 0.5 * ai_count + 0.5 * weight_count
    else:
        # Low confidence: trust weight more
        fused_count = 0.3 * ai_count + 0.7 * weight_count

    fused_count_rounded = round(fused_count)

    # Check agreement between methods
    method_difference = abs(ai_count - weight_count)

    if method_difference < tolerance:
        decision = "OK"
    else:
        decision = "NG"

    return {
        "decision": decision,
        "fused_count": fused_count_rounded,
        "weight_count": weight_count,
        "ai_count": ai_count,
        "ai_confidence": ai_confidence,
        "method_difference": method_difference
    }
```

---

## 🔄 Complete Integration Example

Here's a complete working example that runs both systems:

```python
"""
complete_fusion.py — Full integration of Weight + AI counting
"""
import serial
import json
import time
import torch
import cv2
from datetime import datetime

# ── Configuration ──
SERIAL_PORT = "COM3"
BAUD_RATE = 9600
TOLERANCE = 3
CAMERA_ID = 0

# ── Initialize Hardware ──
ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=2)
cap = cv2.VideoCapture(CAMERA_ID)
time.sleep(2)  # Wait for Arduino reset

# ── Load DME Model ──
model = torch.load("dme_model.pth")
model.eval()

def get_weight_data():
    """Read weight data from Arduino."""
    line = ser.readline().decode("utf-8").strip()
    if line.startswith("{"):
        return json.loads(line)
    return None

def get_ai_count():
    """Get part count from density map estimation."""
    ret, frame = cap.read()
    if not ret:
        return None, 0.0

    # Preprocess frame for model
    tensor = preprocess(frame)  # Your preprocessing function

    with torch.no_grad():
        density_map = model(tensor)
        count = density_map.sum().item()
        confidence = calculate_confidence(density_map)

    return round(count), confidence

def main_loop():
    """Main fusion loop."""
    print("System running... Press Ctrl+C to stop")

    while True:
        # Get weight data
        weight_data = get_weight_data()
        if weight_data is None or not weight_data.get("stable"):
            continue  # Wait for stable reading

        weight_count = weight_data["count_rounded"]

        # Get AI prediction
        ai_count, ai_confidence = get_ai_count()

        # Make decision
        difference = abs(ai_count - weight_count) if ai_count else 0
        decision = "OK" if difference < TOLERANCE else "NG"

        # Output result
        result = {
            "timestamp": datetime.now().isoformat(),
            "weight_count": weight_count,
            "ai_count": ai_count,
            "ai_confidence": ai_confidence,
            "difference": difference,
            "decision": decision,
            "weight_gross": weight_data["weight_gross"]
        }

        print(json.dumps(result, indent=2))

        # Log result
        with open("fusion_log.jsonl", "a") as f:
            f.write(json.dumps(result) + "\n")

if __name__ == "__main__":
    main_loop()
```

---

## 📊 Decision Matrix

| AI Count | Weight Count | Difference | Expected | Decision | Action |
|----------|-------------|------------|----------|----------|--------|
| 65 | 66 | 1 | 65 | ✓ OK | Accept |
| 65 | 66 | 1 | - | ✓ OK | Accept |
| 65 | 70 | 5 | 65 | ✗ NG | Reject, re-count |
| 65 | 70 | 5 | 70 | ⚠ Review | Manual inspection |
| None | 66 | - | 65 | ✓ OK | Weight-only mode |
| None | 66 | - | 60 | ✗ NG | Weight-only reject |
| 65 | None | - | - | ⚠ | Sensor error |

---

## ⚠️ Important Notes

### Timing Synchronization

- Arduino sends data at **2 Hz** (every 500ms)
- DME model may take **100-500ms** per inference
- Ensure both readings are from the **same batch of parts**
- Use timestamps to match readings within a **2-second window**

### Tolerance Selection

| Part Size | Recommended Tolerance |
|-----------|----------------------|
| < 5 parts | 1 |
| 5-20 parts | 2 |
| 20-50 parts | 3 |
| 50-100 parts | 5 |
| 100+ parts | 5% of expected count |

### Edge Cases

1. **Empty platform:** Both should read 0 → OK
2. **Packaging only:** Weight reads packaging weight, AI reads 0 → OK (0 parts)
3. **Parts overlapping:** AI may undercount → Trust weight more
4. **Parts stacked:** AI will undercount → Trust weight more
5. **Very few parts (1-3):** Weight may be unreliable → Trust AI more

---

## 🛠️ Setup Checklist

- [ ] Arduino firmware uploaded and calibrated
- [ ] Python `pyserial` installed (`pip install pyserial`)
- [ ] Camera connected and tested
- [ ] DME model trained and exported
- [ ] `read_serial.py` configured with correct COM port
- [ ] Integration method chosen (file/API/queue)
- [ ] Tolerance values tested with sample batches
- [ ] Logging directory created
- [ ] Full system test with known quantities
