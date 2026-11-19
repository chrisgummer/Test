/*
  Bluetti AC200P L state-of-charge display for the Waveshare ESP32-C6-GEEK.

  Libraries:
    - Adafruit GFX Library
    - Adafruit ST7735 and ST7789 Library
    - PubSubClient
    - ArduinoJson

  Edit the Wi-Fi and MQTT settings below, then subscribe to MQTT_TOPIC.
*/
#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

// Wi-Fi settings
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// MQTT settings
const char* MQTT_HOST = "192.168.1.10";
const uint16_t MQTT_PORT = 1883;
const char* MQTT_USERNAME = "";
const char* MQTT_PASSWORD = "";
const char* MQTT_TOPIC = "bluetti/ac200pl/state";

// ESP32-C6-GEEK onboard ST7789 pins
const int8_t LCD_RST = 4;
const int8_t LCD_DC = 3;
const int8_t LCD_CS = 5;
const int8_t LCD_BL = 6;
const int8_t LCD_SCK = 1;
const int8_t LCD_MOSI = 2;

Adafruit_ST7789 display(&SPI, LCD_CS, LCD_DC, LCD_RST);
WiFiClient networkClient;
PubSubClient mqtt(networkClient);

void drawStatus(const char* message) {
  display.fillScreen(ST77XX_BLACK);
  display.setTextWrap(false);
  display.setTextColor(ST77XX_CYAN);
  display.setTextSize(2);
  display.setCursor(12, 14);
  display.print("Bluetti AC200P L");
  display.drawFastHLine(12, 42, 216, ST77XX_BLUE);
  display.setTextColor(ST77XX_YELLOW);
  display.setTextSize(2);
  display.setCursor(12, 61);
  display.print(message);
}

void drawCharge(float soc) {
  display.fillScreen(ST77XX_BLACK);
  display.setTextWrap(false);
  display.setTextColor(ST77XX_CYAN);
  display.setTextSize(2);
  display.setCursor(12, 12);
  display.print("Bluetti AC200P L");
  display.drawFastHLine(12, 39, 216, ST77XX_BLUE);

  display.setTextColor(ST77XX_WHITE);
  display.setTextSize(2);
  display.setCursor(73, 52);
  display.print("CHARGE");

  char value[12];
  snprintf(value, sizeof(value), "%.0f%%", soc);
  display.setTextColor(soc <= 20 ? ST77XX_RED : ST77XX_GREEN);
  display.setTextSize(4);
  display.setCursor(60, 82);
  display.print(value);
}

void onMqttMessage(char* topic, byte* payload, unsigned int length) {
  (void)topic;

  StaticJsonDocument<256> json;
  DeserializationError error = deserializeJson(json, payload, length);
  if (error) {
    Serial.print("Invalid MQTT JSON: ");
    Serial.println(error.c_str());
    return;
  }

  JsonVariantConst socValue = json["b_soc_total"];
  if (socValue.isNull() || (!socValue.is<int>() && !socValue.is<float>())) {
    Serial.println("MQTT JSON is missing numeric b_soc_total");
    return;
  }

  float soc = socValue.as<float>();
  if (soc < 0.0f || soc > 100.0f) {
    Serial.println("Ignoring b_soc_total outside 0-100");
    return;
  }

  Serial.printf("Battery state of charge: %.1f%%\n", soc);
  drawCharge(soc);
}

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  drawStatus("Connecting Wi-Fi...");

  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
  }

  Serial.printf("\nWi-Fi connected: %s\n", WiFi.localIP().toString().c_str());
}

void connectMqtt() {
  while (!mqtt.connected()) {
    drawStatus("Connecting MQTT...");
    Serial.print("Connecting to MQTT...");

    bool connected;
    if (MQTT_USERNAME[0] != '\0') {
      connected = mqtt.connect("esp32c6-bluetti-display", MQTT_USERNAME, MQTT_PASSWORD);
    } else {
      connected = mqtt.connect("esp32c6-bluetti-display");
    }

    if (connected) {
      Serial.println(" connected");
      if (mqtt.subscribe(MQTT_TOPIC)) {
        Serial.printf("Subscribed to %s\n", MQTT_TOPIC);
        drawStatus("Waiting for data...");
      } else {
        Serial.println("MQTT subscribe failed");
        drawStatus("Subscribe failed");
      }
    } else {
      Serial.printf(" failed (state %d); retrying in 5s\n", mqtt.state());
      drawStatus("MQTT unavailable");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LCD_BL, OUTPUT);
  digitalWrite(LCD_BL, HIGH);

  SPI.begin(LCD_SCK, -1, LCD_MOSI, LCD_CS);
  display.init(135, 240);
  display.setRotation(1);
  drawStatus("Starting...");

  connectWiFi();
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(512);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
  }

  if (!mqtt.connected()) {
    connectMqtt();
  }

  mqtt.loop();
  delay(10);
}
