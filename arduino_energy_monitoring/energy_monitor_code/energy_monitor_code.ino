#define BLYNK_TEMPLATE_ID "TMPL6Wq6wb-77"
#define BLYNK_TEMPLATE_NAME "Energy Meter"
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>
#include "EmonLib.h"
#include <HTTPClient.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);


const char auth[] = "oMKqM3yAO54IYECsf-2b3VLY5zOiOEnp";
const char ssid[] = "GFiber_75CFC"; 
const char pass[] = "PdkfGZvh";


const char* serverName = "http://192.168.1.XX:5000/data";


const char* telegramBotToken = "8496822520:AAF6E3K6_Ot21ggF5hfv3CM6a_R3B15rSTk";
const char* telegramChatID = "8329705509"; 


float vCalibration = 72.0;   
float currCalibration = 0.06; 

EnergyMonitor emon;
BlynkTimer timer;

double kWh = 0.0;
float cost = 0.0;
const float ratePerkWh = 6.5; 
unsigned long lastMillis = 0;
const int resetButtonPin = 4;

float smoothP = 0;
float smoothV = 0;

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  lcd.print("Loading Memory...");

  analogReadResolution(12); 
  EEPROM.begin(32);
  pinMode(resetButtonPin, INPUT_PULLUP);

  WiFi.begin(ssid, pass);
  Blynk.config(auth);

  emon.voltage(35, vCalibration, 1.7);
  emon.current(34, currCalibration);


  
  EEPROM.get(12, kWh);
  if (isnan(kWh) || kWh < 0) kWh = 0; 
  
  lastMillis = millis();

  timer.setInterval(2000L, sendEnergyData);      
  timer.setInterval(2000L, updateLCD); 
  timer.setInterval(5000L, sendToFlask);          
  timer.setInterval(60000L, sendBillToTelegram); 
  
  lcd.clear();
}

void loop() {
  Blynk.run();
  timer.run();
  
  
  if (digitalRead(resetButtonPin) == LOW) {
    delay(500); 
    kWh = 0;
    EEPROM.put(12, kWh);
    EEPROM.commit();
    lcd.clear(); 
    lcd.print("MEMORY WIPED");
    delay(1000);
  }
}

void sendEnergyData() {
  emon.calcVI(30, 2000);

  smoothV = (smoothV * 0.8) + (emon.Vrms * 0.2);
  smoothP = (smoothP * 0.8) + (emon.realPower * 0.2);

  
  if (smoothV < 80.0) smoothV = 0;
  if (smoothP < 10.0) {
    smoothP = 0;
    emon.Irms = 0;
  }

  unsigned long now = millis();
  double durationHours = (double)(now - lastMillis) / 3600000.0;
  
  if (smoothP > 0) {
    double deltaKWh = (smoothP / 1000.0) * durationHours;
    kWh += deltaKWh;
    
    
    static double lastSavedkWh = 0;
    if (abs(kWh - lastSavedkWh) > 0.005) {
      EEPROM.put(12, kWh);
      EEPROM.commit();
      lastSavedkWh = kWh;
    }
  }
  
  lastMillis = now;
  cost = kWh * ratePerkWh;

  Blynk.virtualWrite(V0, smoothV);
  Blynk.virtualWrite(V1, emon.Irms);
  Blynk.virtualWrite(V2, smoothP);
  Blynk.virtualWrite(V3, kWh);
  Blynk.virtualWrite(V4, cost);

  
  
  Serial.print(smoothV); Serial.print(",");
  Serial.print(emon.Irms); Serial.print(",");
  Serial.print(smoothP); Serial.print(",");
  Serial.print(kWh); Serial.print(",");
  Serial.println(cost);
}

// NEW FUNCTION: Sends data to your Flask Website via Wi-Fi
void sendToFlask() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{\"voltage\":" + String(smoothV) + 
                         ",\"current\":" + String(emon.Irms) + 
                         ",\"power\":" + String(smoothP) + 
                         ",\"kwh\":" + String(kWh) + 
                         ",\"cost\":" + String(cost) + "}";

    int httpResponseCode = http.POST(jsonPayload);
    
    if (httpResponseCode > 0) {
      Serial.print("HTTP Success: ");
      Serial.println(httpResponseCode);
    } else {
      Serial.print("HTTP Error: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}

void updateLCD() {
  lcd.setCursor(0,0);
  lcd.printf("V:%.1f P:%.1fW  ", smoothV, smoothP);
  lcd.setCursor(0,1);
  lcd.printf("P%.2f %.3fkWh ", cost, kWh);
}

void sendBillToTelegram() {
  String msg = "⚡ *Energy Report*\n"
               "Voltage: " + String(smoothV, 1) + " V\n"
               "Power: " + String(smoothP, 1) + " W\n"
               "Total: " + String(kWh, 3) + " kWh\n"
               "Cost: ₱" + String(cost, 2);

  HTTPClient http;
  http.begin("https://api.telegram.org/bot" + String(telegramBotToken) + "/sendMessage");
  http.addHeader("Content-Type", "application/json");
  String body = "{\"chat_id\":\"" + String(telegramChatID) + "\", \"text\":\"" + msg + "\", \"parse_mode\":\"Markdown\"}";
  http.POST(body);
  http.end();
}