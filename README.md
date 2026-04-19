# ⚡ SMS: Household Energy Usage Monitor

> A capstone project by **Paul Anthony T. Cabahug**  
> PHINMA University of Iloilo — College of Information Technology Education  
> Bachelor of Science in Information Technology, 2026

A smart, real-time household energy monitoring system built with an **ESP32 microcontroller**, **Python Flask**, and a **web dashboard**. The system measures live voltage, current, and power draw from a single-phase AC outlet, accumulates kWh usage, calculates cost in Philippine Peso (₱), and pushes all data to a local web server — accessible from any browser on the same network.

---

## 📋 Table of Contents

- [System Overview](#system-overview)
- [Hardware Components](#hardware-components)
- [Software Stack](#software-stack)
- [System Architecture](#system-architecture)
- [Features & Functions](#features--functions)
- [API Endpoints](#api-endpoints)
- [File Structure](#file-structure)
- [Running on Localhost (Step-by-Step)](#running-on-localhost-step-by-step)
  - [Step 1 — Set Up the Python Flask Server](#step-1--set-up-the-python-flask-server)
  - [Step 2 — Configure and Flash the ESP32](#step-2--configure-and-flash-the-esp32)
  - [Step 3 — Connect Everything and Test](#step-3--connect-everything-and-test)
- [Calibration Notes](#calibration-notes)
- [Known Limitations](#known-limitations)

---

## System Overview

The Philippines has some of the highest electricity rates in Southeast Asia, and most consumers have no real-time visibility into how much energy each appliance is consuming. This project solves that by building a plug-in energy monitor that shows live readings on an LCD screen, syncs to a local web dashboard, and logs everything to a SQLite database.

The system operates on a **three-layer architecture**:
1. **Sensing Layer** — ESP32 reads voltage and current sensors every 2 seconds
2. **Processing Layer** — ESP32 calculates power, accumulates kWh, and sends JSON to Flask over Wi-Fi
3. **Output Layer** — Flask serves the live web dashboard and stores logs in SQLite, while the ESP32 drives a 16×2 LCD locally

---

## Hardware Components

| Component | Purpose |
|-----------|---------|
| **ESP32 Dev Board** | Main microcontroller — handles all sensing, computation, Wi-Fi, and display logic |
| **ZMPT101B Voltage Sensor** | Isolation transformer module for safe AC voltage measurement (connected to GPIO 35) |
| **ACS712 Current Sensor** | Hall-effect sensor that measures AC current without breaking the circuit (connected to GPIO 34) |
| **16×2 I2C LCD (0x27)** | Local display showing live voltage, power, cost, and kWh |
| **Push Button** | Connected to GPIO 4 with INPUT_PULLUP — resets the accumulated kWh counter and EEPROM data |
| **Power Supply** | 5V USB supply for the ESP32 |

---

## Software Stack

| Layer | Technology |
|-------|-----------|
| Microcontroller Firmware | Arduino IDE (C++) |
| Firmware Libraries | `EmonLib`, `LiquidCrystal_I2C`, `WiFi`, `HTTPClient`, `EEPROM` |
| Backend Server | Python 3 + Flask |
| Database | SQLite3 (local file: `energy_data.db`) |
| Frontend Dashboard | HTML5 + Tailwind CSS + Chart.js |

---

## System Architecture

```
┌──────────────────────────────────────────────┐
│              SENSING LAYER                   │
│  ZMPT101B → GPIO 35   ACS712 → GPIO 34       │
│         ↓ EmonLib calculates VI              │
│              ESP32 (Arduino)                 │
└───────────┬───────────────┬──────────────────┘
            │               │
            ▼               ▼
    ┌───────────┐    ┌──────────────┐
    │ 16x2 LCD  │    │  Wi-Fi POST  │  (every 5 sec)
    │ (I2C)     │    │  JSON →      │
    └───────────┘    │  Flask :5000 │
                     └──────┬───────┘
                            │
            ┌───────────────▼──────────────────┐
            │         FLASK SERVER              │
            │  app_energy.py                    │
            │  - Receives & validates JSON       │
            │  - Writes to SQLite DB             │
            │  - Serves energy_monitor.html      │
            │  - Serves history.html             │
            │  - Exposes REST API endpoints      │
            └──────────────────────────────────┘
                            │
            ┌───────────────▼──────────────────┐
            │         WEB DASHBOARD             │
            │  Browser polls /data every 2s     │
            │  Live chart, peak tracker, cost   │
            └──────────────────────────────────┘
```

---

## Features & Functions

### Live Dashboard (`energy_monitor.html`)
- **Voltage card** — Displays live AC RMS voltage in Volts, updated every 2 seconds
- **Power card** — Displays live real power in Watts; values below 0.8W are displayed as 0.0W to suppress noise
- **Peak Power tracker** — Tracks the highest wattage seen in the current session; persisted in `localStorage` so it survives page refreshes. Includes a "Reset Peak" button
- **Cost card** — Shows accumulated electricity cost in ₱ (Philippine Peso) using the configured rate of ₱6.50/kWh
- **Live Load Profile chart** — A scrolling line chart (Chart.js) showing the last 10 power readings with timestamps. Chart data is preserved in `localStorage` across refreshes
- **History button** — Navigates to the full log table

### History Page (`history.html`)
- **Full log table** — Shows up to 100 most recent entries from the SQLite database (timestamp, voltage, watts, cost)
- **Search / filter** — Live text search that filters visible rows instantly without a page reload
- **CSV Export** — Downloads today's data as a formatted `.csv` file (`energy_report_YYYY-MM-DD.csv`) with columns: Timestamp, Voltage (V), Current (A), Power (W), Usage (kWh), Cost (PHP)
- **Clear All** — Deletes all records from the database; also clears the chart data and peak value from `localStorage` via a cross-tab broadcast using the Storage event

### Flask Backend (`app_energy.py`)
All routes return JSON except for the HTML pages and CSV download.

### ESP32 Firmware (`sketch_jan26aTEST1.ino`)
- **`sendEnergyData()`** — Runs every 2 seconds. Calls `emon.calcVI()` to compute Vrms, Irms, and real power. Applies exponential smoothing (80/20 split) to reduce sensor noise. Calculates delta kWh based on elapsed time and accumulates the total. Saves kWh to EEPROM when the change exceeds 0.005 kWh to minimize flash wear.
- **`sendToFlask()`** — Runs every 5 seconds. Sends a JSON POST request to the Flask server over Wi-Fi with all 5 energy parameters
- **`updateLCD()`** — Runs every 2 seconds. Writes voltage, power, cost, and kWh to the 16×2 LCD
- **Reset Button** — Detected in `loop()`. Holding it LOW for 500ms zeroes out kWh in memory and EEPROM and displays "MEMORY WIPED" on the LCD

---

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| `GET` | `/` | Serves the live dashboard HTML |
| `GET` | `/history` | Serves the history page HTML |
| `GET` | `/status` | Returns Flask status + latest data + server timestamp (JSON) |
| `GET` | `/data` | Returns the latest sensor reading (JSON) |
| `POST` | `/data` | Receives a new sensor reading from the ESP32 (JSON body) |
| `GET` | `/all_logs` | Returns the 100 most recent database entries (JSON) |
| `POST` | `/reset_logs` | Deletes all database records and resets in-memory latest data |
| `GET` | `/download_csv` | Downloads today's logs as a CSV file attachment |

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

---

## File Structure

```
project/
├── app_energy.py                  # Flask server — all routes, DB logic, CORS
├── templates/
│   ├── energy_monitor.html        # Live dashboard (Flask renders this at /)
│   └── history.html               # Historical log viewer
├── sketch_jan26aTEST1.ino         # ESP32 Arduino firmware
├── energy_data.db                 # SQLite database (auto-created on first run)
└── README.md
```

> **Note:** Flask's `render_template()` looks for HTML files inside a `templates/` folder. Make sure `energy_monitor.html` and `history.html` are placed there.

---

## Running on Localhost (Step-by-Step)

### Prerequisites

- Python 3.8 or higher installed
- `pip` available
- Arduino IDE 2.x installed (for flashing the ESP32)
- ESP32 board support installed in Arduino IDE
- All hardware wired and connected via USB

---

### Step 1 — Set Up the Python Flask Server

**1.1 — Clone or download the project files**

Place `app_energy.py` in your project folder. Create a `templates/` subfolder inside it and place both `energy_monitor.html` and `history.html` inside that subfolder:

```
my_project/
├── app_energy.py
└── templates/
    ├── energy_monitor.html
    └── history.html
```

**1.2 — (Optional but recommended) Create a virtual environment**

```bash
python -m venv venv

# On Windows:
venv\Scripts\activate

# On macOS / Linux:
source venv/bin/activate
```

**1.3 — Install required Python packages**

```bash
pip install flask
```

Flask is the only external dependency. `sqlite3`, `threading`, `csv`, and `io` are all part of the Python standard library.

**1.4 — Find your computer's local IP address**

You need this so the ESP32 can reach your Flask server.

```bash
# Windows:
ipconfig
# Look for "IPv4 Address" under your Wi-Fi adapter (e.g. 192.168.1.10)

# macOS:
ipconfig getifaddr en0

# Linux:
hostname -I
```

Write down this IP address — you will use it in the ESP32 firmware.

**1.5 — Run the Flask server**

```bash
python app_energy.py
```

You should see:
```
 * Running on http://0.0.0.0:5000
 * Debug mode: on
```

**1.6 — Open the dashboard in your browser**

Navigate to:
```
http://localhost:5000
```

Or from another device on the same Wi-Fi network:
```
http://192.168.1.XX:5000
```

(Replace `192.168.1.XX` with your actual IP from Step 1.4.)

The dashboard will load and show zeros until the ESP32 starts sending data.

---

### Step 2 — Configure and Flash the ESP32

**2.1 — Install required Arduino libraries**

Open Arduino IDE → go to **Sketch → Include Library → Manage Libraries** and install:

- `EmonLib` by openenergymonitor
- `LiquidCrystal I2C` by Frank de Brabander

**2.2 — Open the firmware file**

Open `sketch_jan26aTEST1.ino` in Arduino IDE.

**2.3 — Update the Wi-Fi credentials**

Find these lines near the top and replace with your network details:

```cpp
const char ssid[] = "YOUR_WIFI_NAME";
const char pass[] = "YOUR_WIFI_PASSWORD";
```

**2.4 — Update the Flask server address**

Find this line and replace `XX` with the last octet of your computer's local IP from Step 1.4:

```cpp
const char* serverName = "http://192.168.1.XX:5000/data";
```

For example, if your computer's IP is `192.168.1.15`:
```cpp
const char* serverName = "http://192.168.1.15:5000/data";
```

**2.5 — (Optional) Adjust the electricity rate**

The default rate is ₱6.50/kWh. Change this if your provider's rate differs:

```cpp
const float ratePerkWh = 6.50;
```

**2.6 — Select the correct board and port**

In Arduino IDE:
- Go to **Tools → Board → ESP32 Arduino → ESP32 Dev Module**
- Go to **Tools → Port** → select the COM port your ESP32 is on

**2.7 — Upload the sketch**

Click the **Upload** button (→). Wait for "Done uploading." Open the Serial Monitor at **115200 baud** to verify it's connecting and sending data. You should see lines like:

```
220.3,0.45,99.1,0.00012,0.00
HTTP Success: 200
```

---

### Step 3 — Connect Everything and Test

**3.1 — Verify data is arriving**

With Flask running and the ESP32 online, visit:
```
http://localhost:5000/status
```

You should see a JSON response with live values being updated.

**3.2 — Check the live dashboard**

Go to `http://localhost:5000` — you should see the voltage, power, and cost cards updating in real time every 2 seconds.

**3.3 — Check the history page**

Click "View History" or go to `http://localhost:5000/history` to see logged entries in the table.

**3.4 — Test the CSV export**

On the history page, click the "📥 CSV" button to download today's data as a spreadsheet-ready CSV file.

**3.5 — Test the reset function**

On the history page, click "Clear All" to wipe the database. The dashboard chart and peak value will also reset.

---

## Calibration Notes

The accuracy of readings depends on two calibration constants in the firmware. These may need adjustment based on your specific sensor modules and wiring:

```cpp
float vCalibration   = 72.0;   // Adjust for accurate voltage readings
float currCalibration = 0.06;  // Adjust for accurate current readings
```

To calibrate:
- Compare the displayed voltage against a known reference (a multimeter set to AC voltage)
- Adjust `vCalibration` up or down until the readings match
- Do the same for current using a known load (e.g. a rated resistive heater)

The firmware also applies noise filtering: voltage readings below 80V are zeroed out, and power readings below 10W are zeroed out to suppress standby noise from the sensors.

---

## Known Limitations

- **Single-phase only** — Designed for standard household AC (single-phase). Not compatible with 3-phase industrial systems.
- **Apparent power** — The system measures apparent power (VA), not true real power (W with power factor correction). For resistive loads (heaters, incandescent bulbs), this is essentially the same. For inductive loads (motors, compressors), there will be a small discrepancy.
- **LAN only** — The Flask server runs on your local network. It is not exposed to the internet by default. For remote access you would need a tunnel (e.g. ngrok) or port forwarding.
- **Single outlet** — The current sensor measures one circuit at a time. Whole-house monitoring would require additional sensors and channels.
- **EEPROM wear** — kWh is saved to EEPROM when the change exceeds 0.005 kWh to minimize flash write cycles, but heavy usage over years may eventually degrade the ESP32's flash memory.

---

## Repository & Links

| Resource | Link |
|----------|------|
| GitHub Repo | https://github.com/ImPaulanthonycabahug/Energy-Monitor.git |
