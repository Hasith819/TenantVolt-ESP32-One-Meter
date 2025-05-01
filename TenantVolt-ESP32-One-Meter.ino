#include <Wire.h>
#include <ZMPT101B.h>
#include "ACS712.h"
#include <LiquidCrystal_I2C.h>
#include <Firebase_ESP_Client.h>
#include "time.h"
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define API_KEY "AIzaSyA4-Ak1g0TuSDYcE8bm04OVxjbxoUTaFFA"
#define DATABASE_URL "https://tenantvolt-default-rtdb.asia-southeast1.firebasedatabase.app/"

#define VOLTAGE_SENSOR_PIN 34
#define SENSITIVITY 500.0f

ZMPT101B voltageSensor(VOLTAGE_SENSOR_PIN, 50.0);

#define ACS_PIN 35
#define VREF 5
#define ADC_RES 4095
#define SENSITIVITY_ACS 100
ACS712 ACS(ACS_PIN, VREF, ADC_RES, SENSITIVITY_ACS);

#define RELAY_PIN 23
bool currentConnectionStatus = true;
unsigned long lastConnectionCheckTime = 0;
const int connectionCheckInterval = 5000;

LiquidCrystal_I2C lcd(0x27, 16, 4);

const char* ssid = "TenantVolt";
const char* password = "20031234";

const int productID = 1119;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 19800;
const int daylightOffset_sec = 0;

unsigned long lastReadingTime = 0;
unsigned long lastSentTime = 0;
const int readingInterval = 500;
const int readingsPerMinute = 120;

float accumulatedPower = 0.0;
int readingCount = 0;
unsigned long lastDataSendTime = 0;
const int dataSendInterval = 60000;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting ESP32...");

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Booting...");

  voltageSensor.setSensitivity(SENSITIVITY);

  ACS.autoMidPoint();

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  Serial.print("Connecting to Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
    if (wifiAttempts > 40) {
      Serial.println("\nFailed to connect to Wi-Fi. Restarting...");
      wifiAttempts = 0;
      ESP.restart();
    }
  }
  Serial.println("\nConnected to Wi-Fi!");

  Serial.println("Initializing Firebase...");
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("Signed In to Firebase ✅");
  } else {
    Serial.println("Firebase Sign-In Failed ❌");
    Serial.println(config.signer.signupError.message.c_str());
    ESP.restart();
  }

  if (Firebase.ready()) {
    Serial.println("Firebase is Ready ✅");
  } else {
    Serial.println("Firebase Initialization Failed ❌");
  }

  Serial.println("Syncing Time...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  delay(2000);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time ❌");
    clearDisplay();
    lcd.print("Failed to obtain time");
  } else {
    Serial.print("Current Time: ");
    Serial.println(asctime(&timeinfo));
  }

  checkConnectionStatus();

  clearDisplay();
  lastDataSendTime = millis();
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastConnectionCheckTime >= connectionCheckInterval) {
    lastConnectionCheckTime = currentMillis;
    checkConnectionStatus();
  }

  if (!currentConnectionStatus) {
    if (currentMillis - lastReadingTime >= readingInterval) {
      lastReadingTime = currentMillis;

        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1");
        lcd.setCursor(0, 1);
        lcd.print("\1\1\1\1 TenantVolt \1\1\1\1");
        lcd.setCursor(-4, 3);
        lcd.print("\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1\1");

        String message = "Service Disconnected! Please pay your bill to restore electricity.";
        int len = message.length();

        for (int i = 0; i < len + 16; i++) {
            lcd.setCursor(-4, 2);
            lcd.print(message.substring(max(0, i - 16), i));
            delay(300);
            lcd.setCursor(-4, 2);
            lcd.print("                ");
        }

      Serial.println("Connection OFF - Not reading sensors");
    }

    if (currentMillis - lastDataSendTime >= dataSendInterval) {
      lastDataSendTime = currentMillis;

      Serial.println("Sending zeros to Firebase (connection OFF)...");

      if (sendDataToFirebase(0.0)) {
        Serial.println("✅ Zero data sent successfully!");
      } else {
        Serial.print("❌ Firebase Error: ");
        Serial.println(fbdo.errorReason());
      }
    }
  }
  else {
    if (currentMillis - lastReadingTime >= readingInterval) {
      lastReadingTime = currentMillis;

      float voltage = voltageSensor.getRmsVoltage();
      float current = ACS.mA_AC();
      float power = voltage * (current / 1000.0);

      accumulatedPower += power;
      readingCount++;

      Serial.print("Voltage: ");
      Serial.print(voltage, 2);
      Serial.print(" V | Current: ");
      Serial.print(current, 2);
      Serial.print(" mA | Power: ");
      Serial.print(power, 2);
      Serial.println(" W");

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("\1 TenantVolt \1\1\1");
      lcd.setCursor(0, 1);
      lcd.print("\1  V : ");
      lcd.print(voltage, 2);
      lcd.setCursor(-4, 2);
      lcd.print("\1  mA: ");
      lcd.print(current, 2);
      lcd.setCursor(-4, 3);
      lcd.print("\1  W : ");
      lcd.print(power, 2);
    }

    if (readingCount >= readingsPerMinute || (currentMillis - lastDataSendTime >= dataSendInterval)) {
      float avgPower = 0.0;

      if (readingCount > 0) {
        avgPower = accumulatedPower / readingCount;
      }

      Serial.print("Sending Average Power: ");
      Serial.print(avgPower, 2);
      Serial.println(" W to Firebase...");

      if (sendDataToFirebase(avgPower)) {
        Serial.println("✅ Data sent successfully!");
      } else {
        Serial.print("❌ Firebase Error: ");
        Serial.println(fbdo.errorReason());
      }

      accumulatedPower = 0.0;
      readingCount = 0;
      lastDataSendTime = currentMillis;
    }
  }
}

void checkConnectionStatus() {
  if (Firebase.ready()) {
    bool connectionStatus = false;
    String path = "/electricity_usage/" + String(productID) + "/connection_status";

    if (Firebase.RTDB.getBool(&fbdo, path.c_str())) {
      connectionStatus = fbdo.to<bool>();

      Serial.print("Connection Status from Firebase: ");
      Serial.println(connectionStatus ? "ON" : "OFF");

      if (connectionStatus != currentConnectionStatus) {
        currentConnectionStatus = connectionStatus;
        updateRelayState();

        accumulatedPower = 0.0;
        readingCount = 0;
      }
    } else {
      Serial.print("Failed to read connection status: ");
      Serial.println(fbdo.errorReason());
    }
  }
}

void updateRelayState() {
  if (currentConnectionStatus) {
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("Power connection ENABLED ⚡");
  } else {
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("Power connection DISABLED ❌");
  }
}

bool sendDataToFirebase(float power) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time ❌");
    return false;
  }

  char datePath[11];
  char hourPath[3];
  char minutePath[3];
  strftime(datePath, sizeof(datePath), "%Y-%m-%d", &timeinfo);
  strftime(hourPath, sizeof(hourPath), "%H", &timeinfo);
  strftime(minutePath, sizeof(minutePath), "%M", &timeinfo);

  String path = "/electricity_usage/" + String(productID) + "/" + String(datePath) + "/" + String(hourPath) + "/" + String(minutePath);
  return Firebase.RTDB.setFloat(&fbdo, path.c_str(), power);
}

void clearDisplay(){
  lcd.clear();
  lcd.setCursor(0, 0);
}