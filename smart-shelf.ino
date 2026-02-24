/*

  Title: SMART Bin
  Author: Jon Hammond
  Description: POC Software for ESP32-C6 SMART Bin
  
*/
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "esp_sleep.h"

static const char* WIFI_SSID = "Wifi-H82z";
static const char* WIFI_PASSWORD = "Owen1028";

// ThingsBoard (change to your host)
static const char* TB_HOST = "mqtt.thingsboard.cloud";
static const uint16_t TB_PORT = 1883;

// Device access token from ThingsBoard (used as MQTT username)
static const char* TB_TOKEN = "Cpsac09J548g0CStGxxO";

// Sleep interval (seconds)
static uint32_t SLEEP_SECONDS = 300;

// ThingsBoard topics
static const char* TOPIC_TELEMETRY = "v1/devices/me/telemetry";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

static void connectWifi() {
  Serial.println();
  Serial.print("[WiFi] Connecting to ");
  Serial.println(WIFI_SSID);

  WiFi.setAutoReconnect(false);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Will try for about 10 seconds (20x 500ms)
  int tryDelay = 500;
  int numberOfTries = 20;

  // Wait for the WiFi event
  while (true) {
    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL: Serial.println("[WiFi] SSID not found"); break;
      case WL_CONNECT_FAILED:
        Serial.print("[WiFi] Failed - WiFi not connected! Reason: ");
        return;
        break;
      case WL_CONNECTION_LOST: Serial.println("[WiFi] Connection was lost"); break;
      case WL_SCAN_COMPLETED: Serial.println("[WiFi] Scan is completed"); break;
      case WL_DISCONNECTED: Serial.println("[WiFi] WiFi is disconnected"); break;
      case WL_CONNECTED:
        Serial.println("[WiFi] WiFi is connected!");
        Serial.print("[WiFi] IP address: ");
        Serial.println(WiFi.localIP());
        return;
        break;
      default:
        Serial.print("[WiFi] WiFi Status: ");
        Serial.println(WiFi.status());
        break;
    }

    delay(tryDelay);

    if (numberOfTries <= 0) {
      Serial.print("[WiFi] Failed to connect to WiFi!");
      // Use disconnect function to force stop trying to connect
      WiFi.disconnect();
      return;
    } else {
      numberOfTries--;
    }
  }
}

static bool connectMqtt() {
  mqttClient.setServer(TB_HOST, TB_PORT);

  int maxTries = 5;
  while (!mqttClient.connected() && maxTries > 0) {
    String clientId = "esp32-" + String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);

    Serial.print("[MQTT] Connecting to ");
    Serial.print(TB_HOST);
    Serial.print(":");
    Serial.println(TB_PORT);

    if (mqttClient.connect(clientId.c_str(), TB_TOKEN, nullptr)) {
      Serial.println("[MQTT] Connected");
      return true;
    }

    Serial.print("[MQTT] Connect failed, rc=");
    Serial.println(mqttClient.state());
    delay(1000);
    maxTries--;
  }

  return false;
}

static bool publishJson(const char* topic, const JsonDocument& doc) {
  char payload[512];
  size_t n = serializeJson(doc, payload, sizeof(payload));
  if (n == 0) return false;
  return mqttClient.publish(topic, payload, n);
}

static void publishStatusAndTelemetry() {
  // --- Telemetry JSON ---
  StaticJsonDocument<512> tele;
  tele["rssi"] = WiFi.RSSI();
  tele["ip"] = WiFi.localIP().toString();

  // esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  // tele["wake"] = wakeupCauseToString(cause);

  // Uptime in ms since boot (this wake cycle)
  tele["uptimeMs"] = (uint32_t)millis();

  // Example: fake sensor value placeholder (replace with your sensor read)
  // e.g. from I2C/BME280/DHT/etc.
  tele["TempC"] = 23.5;
  tele["HumPct"] = 55.0;

  publishJson(TOPIC_TELEMETRY, tele);
}

static void goToDeepSleep(uint32_t sleepSeconds) {
  // Clean shutdown of network stack for lower sleep current
  if (mqttClient.connected()) {
    mqttClient.disconnect();
    delay(50);
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(50);

  // Configure wakeup timer (microseconds)
  uint64_t sleepUs = (uint64_t)sleepSeconds * 1000000ULL;
  esp_sleep_enable_timer_wakeup(sleepUs);

  // Enter deep sleep
  esp_deep_sleep_start();
}

static void flashLED() {
  digitalWrite(LED_BUILTIN, HIGH);

  delay(1000);

  digitalWrite(LED_BUILTIN, LOW);

  delay(1000);
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);

  // LED to show active

  connectWifi();

  if (connectMqtt()) {
    publishStatusAndTelemetry();
  }

  for (int i = 0; i < 10; i++) {
    flashLED();
  }

  goToDeepSleep(SLEEP_SECONDS);
}

void loop() {}
