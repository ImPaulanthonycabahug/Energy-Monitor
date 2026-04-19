import sqlite3
from flask import Flask, jsonify, request, Response, render_template
import threading
import csv
import io
from datetime import datetime

app = Flask(__name__)

DB_FILE = 'energy_data.db'
state_lock = threading.Lock()

latest_data = {
    "voltage": 0.0, "current": 0.0, "power": 0.0, "kwh": 0.0, "cost": 0.0
}

def get_db_connection():
    conn = sqlite3.connect(DB_FILE, check_same_thread=False)
    conn.row_factory = sqlite3.Row
    return conn

def init_db():
    conn = get_db_connection()
    conn.execute('''
        CREATE TABLE IF NOT EXISTS energy_logs (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
            voltage REAL, current REAL, power REAL, kwh REAL, cost REAL
        )
    ''')
    conn.commit()
    conn.close()

init_db()

@app.after_request
def add_cors_headers(response):
    response.headers['Access-Control-Allow-Origin'] = '*'
    response.headers['Access-Control-Allow-Methods'] = 'GET, POST, OPTIONS'
    response.headers['Access-Control-Allow-Headers'] = 'Content-Type'
    return response

@app.route('/')
def index():
    return render_template('energy_monitor.html')

@app.route('/history')
def history_page():
    return render_template('history.html')

@app.route('/status')
def status():
    with state_lock:
        return jsonify({
            "flask": "running",
            "latest_data": latest_data,
            "timestamp": datetime.now().isoformat()
        })

@app.route('/data', methods=['GET', 'POST', 'OPTIONS'])
def handle_data():
    global latest_data

    if request.method == 'OPTIONS':
        return '', 204

    if request.method == 'POST':
        print(f"\n[DEBUG] Incoming POST from: {request.remote_addr}")
        try:
            data = request.get_json(force=True)
            print(f"[DEBUG] Payload: {data}")

            if data is None:
                return jsonify({"status": "error", "message": "No JSON received"}), 400

            pwr  = float(data.get('power', 0))
            curr = float(data.get('current', 0))
            volt = float(data.get('voltage', 0))
            kwh  = float(data.get('kwh', 0))
            cst  = float(data.get('cost', 0))

            if pwr  < 0 or pwr  > 50000: pwr  = 0.0
            if curr < 0 or curr > 1000:  curr = 0.0
            if volt < 0 or volt > 500:   volt = 0.0
            if kwh  < 0: kwh = 0.0
            if cst  < 0: cst = 0.0

            if pwr  < 0.8:  pwr  = 0.0
            if curr < 0.05: curr = 0.0

            with state_lock:
                latest_data = {
                    "voltage": round(volt, 1),
                    "current": round(curr, 3),
                    "power":   round(pwr,  2),
                    "kwh":     round(kwh,  5),
                    "cost":    round(cst,  2)
                }

                conn = get_db_connection()
                conn.execute('''
                    INSERT INTO energy_logs (voltage, current, power, kwh, cost)
                    VALUES (?, ?, ?, ?, ?)
                ''', (latest_data['voltage'], latest_data['current'],
                      latest_data['power'],   latest_data['kwh'], latest_data['cost']))
                conn.commit()
                conn.close()

            print(f"[SUCCESS] Updated: {latest_data}")
            return jsonify({"status": "success", "stored": latest_data}), 200

        except Exception as e:
            print(f"[ERROR] {e}")
            return jsonify({"status": "error", "message": str(e)}), 400

    with state_lock:
        return jsonify(latest_data)

@app.route('/all_logs')
def all_logs():
    conn = get_db_connection()
    query = """
        SELECT id, datetime(timestamp, 'localtime') as timestamp,
               voltage, current, power, kwh, cost
        FROM energy_logs ORDER BY id DESC LIMIT 100
    """
    rows = conn.execute(query).fetchall()
    conn.close()
    return jsonify({"logs": [dict(row) for row in rows]})

@app.route('/reset_logs', methods=['POST'])
def reset_logs():
    conn = get_db_connection()
    conn.execute('DELETE FROM energy_logs')
    conn.commit()
    conn.close()
    global latest_data
    with state_lock:
        latest_data = {"voltage": 0.0, "current": 0.0, "power": 0.0, "kwh": 0.0, "cost": 0.0}
    return jsonify({"status": "success"})

@app.route('/download_csv')
def download_csv():
    try:
        today = datetime.now().strftime('%Y-%m-%d')
        conn = get_db_connection()
        query = """
            SELECT datetime(timestamp, 'localtime'), voltage, current, power, kwh, cost
            FROM energy_logs
            WHERE DATE(timestamp, 'localtime') = DATE('now', 'localtime')
            ORDER BY timestamp ASC
        """
        rows = conn.execute(query).fetchall()
        conn.close()

        output = io.StringIO()
        writer = csv.writer(output)
        writer.writerow(['Timestamp', 'Voltage (V)', 'Current (A)', 'Power (W)', 'Usage (kWh)', 'Cost (PHP)'])
        for row in rows:
            writer.writerow(list(row))

        response = Response(output.getvalue(), mimetype='text/csv')
        response.headers.set("Content-Disposition", "attachment", filename=f"energy_report_{today}.csv")
        return response
    except Exception as e:
        return str(e), 500

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)