#include <WiFi.h>
#include <FirebaseESP32.h>
#include "DHT.h"

// 1. Connection Credentials

#define WIFI_SSID "school"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// Database URL (without https:// and trailing /)
#define FIREBASE_HOST "https://console.firebase.google.com/u/1/project/intelligent-greenhouse-network/database/intelligent-greenhouse-network-default-rtdb/data/~2F"
// Database Secret from Project Settings -> Service Accounts -> Database secrets
#define FIREBASE_AUTH "mg6R2YJ5DFBciqxSS8n4sixYz5Ayo7u9FwKb2SpD"

// 2. Hardware Pin Definitions
#define DHTPIN 4       // DHT Sensor Data Pin
#define DHTTYPE DHT11  // Sensor type: DHT11 or DHT22
#define FAN_PIN 18     // Relay Pin for Fan
#define PUMP_PIN 19    // Relay Pin for Water Pump
#define LED_PIN 21     // Relay Pin for Grow Lights

DHT dht(DHTPIN, DHTTYPE);
FirebaseData firebaseData;

// Global variables to store setpoints fetched from Firebase
int tempMax, tempMin, humMax, humMin;

void setup() {
  Serial.begin(115200);

  // Initialize output pins
  pinMode(FAN_PIN, OUTPUT);
  pinMode(PUMP_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);

  // Initialize DHT sensor
  dht.begin();

  // Connect to Wi-Fi network
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to Wi-Fi!");

  // Initialize Firebase connection
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
}

void loop() {

  // 3. Fetching User Selection (The Switcher)
  // ---------------------------------------------------------
  // Check which plant is currently selected on the Website/Firebase
  if (Firebase.getString(firebaseData, "/selected-plant")) {
    String currentPlant = firebaseData.stringData();
    Serial.print("Active Profile: "); Serial.println(currentPlant);

    // Build the dynamic path based on the selected plant (e.g., /Plant-settings/Tomatoes)
    String path = "/Plant-settings/" + currentPlant;

    // Fetch the specific setpoints for the selected plant
    if (Firebase.getInt(firebaseData, path + "/Temp_max")) tempMax = firebaseData.intData();
    if (Firebase.getInt(firebaseData, path + "/Temp_min")) tempMin = firebaseData.intData();
    if (Firebase.getInt(firebaseData, path + "/Hum_max"))  humMax = firebaseData.intData();
    if (Firebase.getInt(firebaseData, path + "/Hum_min"))  humMin = firebaseData.intData();
  }

  // 4. Sensor Reading & Closed-loop Control Logic
  float t = dht.readTemperature(); // Current Temperature
  float h = dht.readHumidity();    // Current Humidity

  // Safety check: skip logic if sensor reading fails
  if (isnan(t) || isnan(h)) {
    Serial.println("Error: Failed to read from DHT sensor!");
    return;
  }

  // Temperature Control (Fan Logic)
  if (t > tempMax) {
    digitalWrite(FAN_PIN, HIGH); // Too hot: Turn ON cooling fan
    Serial.println("Action: Fan ON");
  } else if (t < tempMin) {
    digitalWrite(FAN_PIN, LOW);  // Temperature safe: Turn OFF fan
    Serial.println("Action: Fan OFF");
  }

  // Humidity Control (Pump Logic)
  if (h < humMin) {
    digitalWrite(PUMP_PIN, HIGH); // Soil/Air dry: Start irrigation
    Serial.println("Action: Pump ON");
  } else if (h > humMax) {
    digitalWrite(PUMP_PIN, LOW);  // Humidity sufficient: Stop irrigation
    Serial.println("Action: Pump OFF");
  }

  // 5. Data Logging (Sending Real-time data to Firebase)
  // ---------------------------------------------------------
  // This data can be used for the Website's live charts/graphs
  Firebase.setFloat(firebaseData, "/Live-Data/Temperature", t);
  Firebase.setFloat(firebaseData, "/Live-Data/Humidity", h);

  Serial.println("-----------------------");
  delay(3000); // Wait 3 seconds before next cycle
}
