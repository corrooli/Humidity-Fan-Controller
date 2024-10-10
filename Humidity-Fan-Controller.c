#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>
#include "DHTesp.h" // Using the DHTesp library by beegee-tokyo

// Disable Wi-Fi to prevent interference if not used
void disableWiFi() {
  WiFi.mode(WIFI_OFF);
  WiFi.disconnect(true, true);
  delay(1);
}

// Define GPIO pins
#define DHTPIN 4
#define RELAYPIN 16  // Changed from GPIO15 to GPIO16
#define LEDPIN 2     // Onboard LED is typically on GPIO 2 for ESP32

// Initialize DHT sensor
DHTesp dht;

// Initialize LCD
LiquidCrystal_PCF8574 lcd(0x27);

// Humidity thresholds for hysteresis
const float humidityThresholdOn = 70.0;
const float humidityThresholdOff = 68.0;

// Timing variables
unsigned long previousMillis = 0;
const unsigned long interval = 10000; // 10 seconds

// Track the current state of the relay
bool relayState = false;

void setup() {
  Serial.begin(115200);

  disableWiFi();

  dht.setup(DHTPIN, DHTesp::DHT22);

  pinMode(RELAYPIN, OUTPUT);
  digitalWrite(RELAYPIN, HIGH); // Set to HIGH to ensure relay is OFF at start (active low)

  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, LOW); // Turn off the LED at start

  lcd.begin(16, 2);
  lcd.setBacklight(255);
  lcd.clear();

  // Initial display
  lcd.setCursor(0, 0);
  lcd.print("Temperatur/Feucht");

  Serial.println("Setup complete.");
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    TempAndHumidity data = dht.getTempAndHumidity();

    if (dht.getStatus() != DHTesp::ERROR_NONE) {
      Serial.print("DHT Error: ");
      Serial.println(dht.getStatusString());

      digitalWrite(LEDPIN, HIGH);
      delay(500);
      digitalWrite(LEDPIN, LOW);
      delay(500);

      // Display error on LCD
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Sensorfehler");
      lcd.setCursor(0, 1);
      lcd.print("Versuche erneut...");

      return;
    }

    float humidity = data.humidity;
    float temperature = data.temperature;

    Serial.print("Feuchtigkeit: ");
    Serial.print(humidity);
    Serial.print("%  Temperatur: ");
    Serial.print(temperature);
    Serial.println("°C");

    // Display readings on LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(temperature, 0); // No decimal places
    lcd.print((char)223);      // Degree symbol
    lcd.print("C / ");
    lcd.print(humidity, 0);    // No decimal places
    lcd.print("%");

    // Control relay based on humidity
    if (humidity > humidityThresholdOn && !relayState) {
      lcd.setCursor(0, 1);
      lcd.print("Luftabzug Ein"); // Display "Fan On"
      digitalWrite(RELAYPIN, LOW); // Activate relay (active low)
      digitalWrite(LEDPIN, HIGH);  // Turn on LED
      Serial.println("Relay ON");
      relayState = true;
    } 
    else if (humidity < humidityThresholdOff && relayState) {
      lcd.setCursor(0, 1);
      lcd.print("Luftabzug Aus"); // Display "Fan Off"
      digitalWrite(RELAYPIN, HIGH); // Deactivate relay (active low)
      digitalWrite(LEDPIN, LOW);    // Turn off LED
      Serial.println("Relay OFF");
      relayState = false;
    }

    // If humidity is between thresholds, maintain current state without updating display
  }

  // Other non-blocking tasks can be added here
}
