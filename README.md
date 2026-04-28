# ⚡ SMS: Household Energy Usage Monitor

> A capstone project by **Paul Anthony T. Cabahug**
> PHINMA University of Iloilo — College of Information Technology Education
> Bachelor of Science in Information Technology, 2026

---

## 📌 What Is This?

**SMS (Smart Monitoring System)** is a real-time household energy monitoring solution built from the ground up using an **ESP32 microcontroller**, a **Python Flask backend**, and a **browser-based web dashboard**. The system plugs into a standard single-phase AC outlet and continuously measures voltage, current, and power draw — accumulating kWh usage and calculating the electricity cost in **Philippine Peso (₱)**.

All data is transmitted wirelessly over Wi-Fi every 5 seconds to a locally hosted web server, where it's stored in a SQLite database and displayed on a live dashboard accessible from any browser on the same network. A 16×2 LCD screen attached to the ESP32 provides an at-a-glance local readout without needing a phone or computer.

This project was built to address a real problem: **the Philippines has some of the highest electricity rates in Southeast Asia**, yet most household consumers have zero visibility into how much energy individual appliances are actually consuming. SMS makes that visible — in real time.

---

## 📋 Table of Contents

- [Features](#-features)
- [System Architecture](#-system-architecture)
- [Hardware Components](#-hardware-components)
- [Software Stack](#-software-stack)
- [Project File Structure](#-project-file-structure)
- [API Reference](#-api-reference)
- [Running on Localhost — Step by Step](#-running-on-localhost--step-by-step)
  - [Step 1 — Set Up the Python Flask Server](#step-1--set-up-the-python-flask-server)
  - [Step 2 — Configure and Flash the ESP32](#step-2--configure-and-flash-the-esp32)
  - [Step 3 — Wire the Hardware](#step-3--wire-the-hardware)
  - [Step 4 — Test Everything Together](#step-4--test-everything-together)
- [Calibration Notes](#-calibration-notes)
- [Sample Data](#-sample-data)
- [Known Limitations](#-known-limitations)

---

## ✅ Features

### 🔴 Live Dashboard (`energy_monitor.html`)
- **Voltage card** — Displays live AC RMS voltage in Volts, refreshed every 2 seconds
- **Power card** — Displays real power in Watts; values below 0.8W are shown as 0.0W to suppress sensor noise at idle
- **Peak Power tracker** — Records the highest wattage seen during the current session; stored in `localStorage` so it survives page refreshes. Includes a manual "Reset Peak" button
- **Cost card** — Shows cumulative electricity cost in ₱ based on a configurable rate (default: ₱6.50/kWh)
- **Live Load Profile chart** — A scrolling Chart.js line graph showing the last 10 power readings with timestamps; chart data persists across page refreshes via `localStorage`
- **History button** — One-click navigation to the full log table

### 📜 History Page (`history.html`)
- **Full log table** — Displays up to 100 of the most recent entries from the SQLite database (timestamp, voltage, power, cost)
- **Live search / filter** — Instantly filters visible table rows by any text without a page reload
- **CSV Export** — Downloads today's complete data log as a formatted `.csv` file (`energy_report_YYYY-MM-DD.csv`) with columns: Timestamp, Voltage (V), Current (A), Power (W), Usage (kWh), Cost (PHP)
- **Clear All** — Wipes all records from the database and broadcasts a reset signal via `localStorage` to clear the chart and peak value on the dashboard — even if it's open in another tab

### ⚙️ Flask Backend (`app_energy.py`)
- Receives validated JSON from the ESP32 over HTTP POST every 5 seconds
- Applies server-side sanity checks on all incoming values (e.g. voltage capped at 500V, negative values zeroed out)
- Stores every reading in a local SQLite database (`energy_data.db`) — auto-created on first run
- Serves both the live dashboard and history page using Flask's template system
- Exposes a clean REST API for all frontend interactions
- Full CORS support so the dashboard can run from any origin on the network

### 🔧 ESP32 Firmware (`energy_monitor_code.ino`)
- **`sendEnergyData()`** — Runs every 2 seconds. Computes Vrms, Irms, and real power using EmonLib's `calcVI()`. Applies 80/20 exponential smoothing to reduce noise. Accumulates kWh based on elapsed time and saves to EEPROM when the change exceeds 0.005 kWh (to minimize flash wear)
- **`sendToFlask()`** — Runs every 5 seconds. Transmits a JSON payload with all 5 energy readings to the Flask server via HTTP POST over Wi-Fi
- **`updateLCD()`** — Runs every 2 seconds. Writes live voltage, power, cost, and kWh to the 16×2 I2C LCD screen
- **`sendBillToTelegram()`** — Runs every 60 seconds. Sends a formatted energy report to a configured Telegram chat via the Telegram Bot API
- **Reset button** — Monitored in `loop()`. Holding the button LOW for 500ms zeroes out kWh in both RAM and EEPROM, and displays "MEMORY WIPED" on the LCD
- **Blynk integration** — All 5 readings are pushed to Blynk virtual pins (V0–V4) for optional mobile app monitoring

---

## 🏗️ System Architecture

The system runs on a three-layer architecture:

```
┌──────────────────────────────────────────────┐
│              SENSING LAYER                   │
│  ZMPT101B → GPIO 35   ACS712 → GPIO 34       │
│         ↓ EmonLib calculates VI              │
│              ESP32 (Arduino C++)             │
└───────────┬───────────────┬──────────────────┘
            │               │
            ▼               ▼
    ┌───────────┐    ┌──────────────────┐
    │ 16x2 LCD  │    │  Wi-Fi POST      │  every 5 sec
    │  (I2C)    │    │  JSON payload →  │
    └───────────┘    │  Flask :5000     │
                     └────────┬─────────┘
                              │
              ┌───────────────▼──────────────────┐
              │         FLASK SERVER              │
              │  app_energy.py                    │
              │  ├─ Validates & stores JSON        │
              │  ├─ Writes to SQLite DB            │
              │  ├─ Serves energy_monitor.html     │
              │  ├─ Serves history.html            │
              │  └─ Exposes REST API               │
              └──────────────────────────────────┘
                              │
              ┌───────────────▼──────────────────┐
              │         WEB DASHBOARD             │
              │  Browser polls /data every 2s     │
              │  Live chart, peak tracker, cost   │
              └──────────────────────────────────┘
```

**Layer 1 — Sensing:** The ESP32 reads the ZMPT101B voltage sensor (GPIO 35) and ACS712 current sensor (GPIO 34) using the EmonLib library, computing RMS values and real power.

**Layer 2 — Processing:** The ESP32 calculates delta kWh, accumulates totals, applies smoothing, and transmits to Flask over the local Wi-Fi network.

**Layer 3 — Output:** Flask persists all data to SQLite and serves the web dashboard. The ESP32 simultaneously drives the LCD for local display and optionally pushes data to Blynk and Telegram.

---

## 🔩 Hardware Components

| Component | Role | GPIO |
|-----------|------|------|
| **ESP32 Dev Board** | Main microcontroller — sensing, computation, Wi-Fi, display | — |
| **ZMPT101B Voltage Sensor** | Isolation transformer module for safe AC voltage measurement | 35 |
| **ACS712 Current Sensor** | Hall-effect sensor for non-invasive AC current measurement | 34 |
| **16×2 I2C LCD (addr 0x27)** | Local display showing voltage, power, kWh, and cost | SDA/SCL |
| **Push Button** | Resets accumulated kWh counter and EEPROM data | 4 (PULLUP) |
| **5V USB Power Supply** | Powers the ESP32 dev board | — |

---

## 🧰 Software Stack

| Layer | Technology |
|-------|-----------|
| Microcontroller Firmware | Arduino IDE 2.x (C++) |
| Firmware Libraries | `EmonLib`, `LiquidCrystal_I2C`, `WiFi`, `HTTPClient`, `EEPROM`, `BlynkSimpleEsp32` |
| Backend Server | Python 3.8+ with Flask |
| Database | SQLite3 (file: `energy_data.db`, auto-created) |
| Frontend | HTML5, Tailwind CSS (CDN), Chart.js (CDN) |
| Notifications | Telegram Bot API, Blynk IoT Platform |

---

## 📁 Project File Structure

```
project/
├── app_energy.py                   # Flask server — all routes, DB logic, CORS
├── templates/
│   ├── energy_monitor.html         # Live dashboard (served at /)
│   └── history.html                # Historical log viewer (served at /history)
├── arduino_energy_monitoring/
│   └── energy_monitor_code/
│       └── energy_monitor_code.ino # ESP32 Arduino firmware
├── energy_report_20260215.csv      # Sample exported data log (~45,000 readings)
├── energy_data.db                  # SQLite database (auto-generated on first run)
└── README.md
```

> ⚠️ Flask's `render_template()` requires HTML files to be inside a `templates/` subfolder. Do not move them.

---

## 🔌 API Reference

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/` | Serves the live dashboard HTML |
| `GET` | `/history` | Serves the history page HTML |
| `GET` | `/status` | Returns Flask status, latest reading, and server timestamp |
| `GET` | `/data` | Returns the most recent sensor reading as JSON |
| `POST` | `/data` | Accepts a new sensor reading from the ESP32 |
| `GET` | `/all_logs` | Returns the 100 most recent database rows as JSON |
| `POST` | `/reset_logs` | Deletes all records and resets in-memory state |
| `GET` | `/download_csv` | Downloads today's data as a `.csv` file attachment |

**POST `/data` — expected JSON body:**
```json
{
  "voltage": 220.5,
  "current": 0.45,
  "power": 99.2,
  "kwh": 0.00275,
  "cost": 0.02
}
```

**GET `/data` — example response:**
```json
{
  "voltage": 220.5,
  "current": 0.450,
  "power": 99.20,
  "kwh": 0.00275,
  "cost": 0.02
}
```

---

## 🚀 Running on Localhost — Step by Step

### Prerequisites

Before starting, make sure you have the following installed and ready:

- ✅ Python 3.8 or higher
- ✅ `pip` (comes with Python)
- ✅ Arduino IDE 2.x
- ✅ ESP32 board support installed in Arduino IDE
- ✅ All hardware components wired and ESP32 connected via USB

---

### Step 1 — Set Up the Python Flask Server

**1.1 — Organize the project files**

Your folder structure must look exactly like this before running anything:

```
my_project/
├── app_energy.py
└── templates/
    ├── energy_monitor.html
    └── history.html
```

**1.2 — Create a virtual environment (recommended)**

```bash
# Create the environment
python -m venv venv

# Activate it — Windows:
venv\Scripts\activate

# Activate it — macOS / Linux:
source venv/bin/activate
```

**1.3 — Install Flask**

```bash
pip install flask
```

That's the only external dependency. Everything else (`sqlite3`, `threading`, `csv`, `io`) is part of Python's standard library.

**1.4 — Find your computer's local IP address**

The ESP32 needs this IP to send data to your Flask server. Run the appropriate command:

```bash
# Windows:
ipconfig
# Look for "IPv4 Address" under your Wi-Fi adapter — e.g. 192.168.1.15

# macOS:
ipconfig getifaddr en0

# Linux:
hostname -I
```

Write this IP down — you'll use it in the ESP32 firmware in Step 2.

**1.5 — Run the Flask server**

From inside your project folder (with your virtual environment active):

```bash
python app_energy.py
```

Expected output:
```
 * Running on http://0.0.0.0:5000
 * Debug mode: on
```

**1.6 — Open the dashboard**

Open your browser and go to:

```
http://localhost:5000
```

From another device on the same Wi-Fi network:

```
http://192.168.1.XX:5000
```

Replace `192.168.1.XX` with your actual IP from Step 1.4. The dashboard will load and show all zeros until the ESP32 starts sending data.

---

### Step 2 — Configure and Flash the ESP32

**2.1 — Install required Arduino libraries**

In Arduino IDE, go to **Sketch → Include Library → Manage Libraries** and install:

- `EmonLib` by openenergymonitor
- `LiquidCrystal I2C` by Frank de Brabander
- `Blynk` by Volodymyr Shymanskyy

**2.2 — Open the firmware file**

Open `energy_monitor_code.ino` in Arduino IDE.

**2.3 — Update your Wi-Fi credentials**

Find these two lines near the top and replace with your network details:

```cpp
const char ssid[] = "YOUR_WIFI_NAME";
const char pass[] = "YOUR_WIFI_PASSWORD";
```

**2.4 — Set the Flask server address**

Find this line and replace `XX` with the last part of your computer's local IP from Step 1.4:

```cpp
const char* serverName = "http://192.168.1.XX:5000/data";
```

For example, if your IP is `192.168.1.15`:

```cpp
const char* serverName = "http://192.168.1.15:5000/data";
```

> ⚠️ Make sure your computer and ESP32 are on the **same Wi-Fi network**.

**2.5 — (Optional) Adjust the electricity rate**

The default is ₱6.50/kWh. Update this to match your provider's actual rate:

```cpp
const float ratePerkWh = 6.50;
```

**2.6 — Select the correct board and port**

- Go to **Tools → Board → ESP32 Arduino → ESP32 Dev Module**
- Go to **Tools → Port** and select the COM port your ESP32 is on

**2.7 — Upload the firmware**

Click the **Upload (→)** button and wait for "Done uploading." Open the Serial Monitor at **115200 baud** to verify. You should see:

```
220.3,0.45,99.1,0.00012,0.00
HTTP Success: 200
```

---

### Step 3 — Wire the Hardware

Connect components to the ESP32 using this pinout:

| Component | ESP32 Pin |
|-----------|-----------|
| ZMPT101B output (voltage sensor) | GPIO **35** |
| ACS712 output (current sensor) | GPIO **34** |
| LCD SDA | GPIO **21** |
| LCD SCL | GPIO **22** |
| Reset button (one leg) | GPIO **4** |
| Reset button (other leg) | **GND** |

> ⚠️ **Safety Warning:** The ZMPT101B module has mains AC voltage on its primary side. Ensure the module is fully enclosed and never touched while the system is connected to an outlet. Never connect or disconnect AC wiring while the circuit is live.

---

### Step 4 — Test Everything Together

**4.1 — Verify data is arriving**

With Flask running and the ESP32 online, go to:

```
http://localhost:5000/status
```

You should see a JSON response with non-zero values updating every few seconds.

**4.2 — Check the live dashboard**

Go to `http://localhost:5000` — voltage, power, and cost cards should update every 2 seconds.

**4.3 — Check the history page**

Click "View History" or go to `http://localhost:5000/history` to see the log table growing in real time.

**4.4 — Test the CSV export**

On the history page, click **📥 CSV** — your browser should download `energy_report_YYYY-MM-DD.csv` with today's readings.

**4.5 — Test the database reset**

Click **Clear All** on the history page and confirm the prompt. The database is wiped and the dashboard chart and peak value reset automatically, even in another open browser tab.

**4.6 — Test the hardware reset button**

While the ESP32 is running, hold the physical push button down for 500ms. The LCD should briefly show `MEMORY WIPED` and the kWh counter should return to zero.

---

## 🎛️ Calibration Notes

Sensor accuracy depends on two calibration constants in the firmware. These **will likely need adjustment** depending on your specific sensor modules and wiring:

```cpp
float vCalibration    = 72.0;   // Adjust until voltage matches a multimeter
float currCalibration = 0.06;   // Adjust until current matches a known load
```

**To calibrate voltage:**
1. Connect the system to an outlet
2. Measure the same outlet with a multimeter set to AC Voltage
3. Adjust `vCalibration` up (to increase the reading) or down until both values match

**To calibrate current:**
1. Plug in a known resistive load (e.g. a rated incandescent bulb or resistive heater)
2. Calculate expected current: `I = P ÷ V` (e.g. 100W ÷ 220V ≈ 0.45A)
3. Adjust `currCalibration` until the displayed value matches

The firmware also applies noise floor thresholds — adjust these if you're seeing spurious readings at idle:

```cpp
if (smoothV < 80.0) smoothV = 0;   // Zero out voltage below 80V
if (smoothP < 10.0) smoothP = 0;   // Zero out power below 10W
```

---

## 📊 Sample Data

The file `energy_report_20260215.csv` in this repository contains a real export of **45,544 rows** of sensor readings captured on February 15, 2026. It demonstrates the CSV export format and can be used to test data analysis, charting tools, or visualization pipelines.

**CSV column structure:**

| Column | Unit | Description |
|--------|------|-------------|
| Timestamp | YYYY-MM-DD HH:MM:SS | Local time of the reading |
| Voltage (V) | Volts | AC RMS voltage |
| Current (A) | Amperes | AC RMS current |
| Power (W) | Watts | Real power draw |
| Usage (kWh) | Kilowatt-hours | Accumulated energy usage |
| Cost (PHP) | Philippine Peso | Accumulated cost at ₱6.50/kWh |

---

## ⚠️ Known Limitations

- **Single-phase only** — Designed for standard Philippine household AC (single-phase, 220V). Not compatible with 3-phase industrial systems.
- **Apparent vs. real power** — For resistive loads (heaters, incandescent bulbs) readings are accurate. For inductive loads (motors, compressors, inverter ACs), a small discrepancy exists because power factor is not accounted for.
- **LAN only** — The Flask server is not internet-accessible by default. For remote access you'd need a tunneling tool like `ngrok` or to configure port forwarding on your router.
- **Single outlet** — The current sensor monitors one circuit. Whole-house monitoring would require additional ACS712 channels and firmware modifications.
- **EEPROM wear** — kWh is written to flash only when the accumulated change exceeds 0.005 kWh to limit write frequency. Very heavy long-term use may still gradually wear the ESP32's flash over years.
- **Wi-Fi dependency** — If the ESP32 loses its Wi-Fi connection, readings will no longer reach Flask or the database. The LCD display and EEPROM accumulation continue to work locally.
