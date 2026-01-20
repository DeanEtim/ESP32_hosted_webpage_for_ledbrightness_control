/*
  - ESP32 IoT Monitoring and Control Dashboard 
  - Radial gauge shows potentiometer value in real time (0–100%)
  - Red LED on GPIO pin 15 controlled by a toggle switch on the webpage
  - Blue LED on GPIO pin 21 brightness controlled by a blue slider (0–100%)
  - Potentiometer on GPIO pin 34 of the ESP32

  Notes:
    Access Point: 
    - SSID: "Embedded System Trainer Group 6" 
    - PASSWORD: "group6"
    - to access the webpage, use the link http://192.168.4.1
*/

#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include "htmlWebpage.h"

// Pins on the ESP32
constexpr uint8_t redLED_PIN = 15;
constexpr uint8_t blueLED_PIN = 21;
constexpr uint8_t analogSensor = 34;

// LED PWM parameters
constexpr uint8_t channel = 0;
constexpr uint16_t frequency = 5000;  // Hz
constexpr uint8_t resolution = 8;     // bits
uint32_t lastPotMs = 0;

// Wi-Fi credentials
const char* ssid = "Embedded System Trainer Group 6";
const char* password = "group6";

// local Servers
WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// Status
volatile bool redLedStatus = false;   // default OFF
volatile uint8_t blueLedStatus = 50;  // default 50%
volatile uint16_t potRaw = 0;

void broadcastState() {
  StaticJsonDocument<128> doc;
  doc["led1"] = redLedStatus;
  doc["brightness"] = blueLedStatus;
  String out;
  serializeJson(doc, out);
  webSocket.broadcastTXT(out);
}  // Broadcast current state to all clients

void broadcastPot(uint8_t pct) {
  StaticJsonDocument<64> doc;
  doc["potPct"] = pct;
  String out;
  serializeJson(doc, out);
  webSocket.broadcastTXT(out);
}  // Send analog sensor value (in percentage)

void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      {
        // Send current state to the newly connected client
        StaticJsonDocument<128> doc;
        doc["led1"] = redLedStatus;
        doc["brightness"] = blueLedStatus;
        String out;
        serializeJson(doc, out);
        webSocket.sendTXT(num, out);
        break;
      }

    case WStype_TEXT:
      {
        StaticJsonDocument<128> in;
        DeserializationError err = deserializeJson(in, payload, length);
        if (err) return;
        const char* t = in["type"] | "";
        if (!strcmp(t, "hello")) {
          // client requested state again
          StaticJsonDocument<128> doc;
          doc["led1"] = redLedStatus;
          doc["brightness"] = blueLedStatus;
          String out;
          serializeJson(doc, out);
          webSocket.sendTXT(num, out);
        } else if (!strcmp(t, "set_led1")) {
          bool v = in["value"] | false;
          redLedStatus = v;
          digitalWrite(redLED_PIN, redLedStatus ? HIGH : LOW);
          broadcastState();
        } else if (!strcmp(t, "set_brightness")) {
          int v = in["value"] | 0;
          v = constrain(v, 0, 100);
          blueLedStatus = (uint8_t)v;
          uint32_t duty = map(blueLedStatus, 0, 100, 0, 255);
          ledcWrite(channel, duty);
          broadcastState();
        }
        break;
      }
    default: break;
  }
}  // WebSocket event handler

void handleRoot() {
  server.send_P(200, "text/html; charset=utf-8", webpage_HTML);
}  // HTTP handler

void setup() {
  Serial.begin(115200);

  // GPIO setup
  pinMode(redLED_PIN, OUTPUT);
  digitalWrite(redLED_PIN, LOW);  // default OFF

  // analogWrite setup
  ledcSetup(channel, frequency, resolution);
  ledcAttachPin(blueLED_PIN, channel);
  ledcWrite(channel, map(blueLedStatus, 0, 100, 0, 255));

  // ADC setup
  analogReadResolution(12);
  analogSetPinAttenuation(analogSensor, ADC_11db);

  // WiFi AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.println(WiFi.softAPIP());
  delay(2000);

  // Start servers
  server.on("/", handleRoot);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(onWsEvent);

  Serial.println("WebSocket Server started!");
  Serial.println("Connect via: http://192.168.4.1");
}

void loop() {
  server.handleClient();
  webSocket.loop();

  uint32_t now = millis();
  if (now - lastPotMs >= 50) {
    lastPotMs = now;
    potRaw = analogRead(analogSensor); 

    static float filt = 0;
    float pct = (potRaw / 4095.0f) * 100.0f;
    filt = 0.7f * filt + 0.3f * pct;  // simple low-pass
    uint8_t sendPct = (uint8_t)round(constrain(filt, 0, 100));
    broadcastPot(sendPct);
  }
}
