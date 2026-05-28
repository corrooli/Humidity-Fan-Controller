#include <Wire.h>
#include "Adafruit_SHT4x.h"

// ==========================================
// PIN CONFIGURATION
// ==========================================
#define RELAY_PIN  18
#define BUTTON_PIN 14
#define SDA_PIN    21
#define SCL_PIN    22

// ==========================================
// RELAY POLARITY
// ==========================================
#define RELAY_ACTIVE_HIGH true
const uint8_t RELAY_ON  = RELAY_ACTIVE_HIGH ? HIGH : LOW;
const uint8_t RELAY_OFF = RELAY_ACTIVE_HIGH ? LOW  : HIGH;

// ==========================================
// SYSTEM THRESHOLDS & SETTINGS
// ==========================================
const float HUMIDITY_MAX   = 65.0;   // fan ON at/above this %RH
const float HUMIDITY_MIN   = 55.0;   // fan OFF at/below this %RH
const float HUMIDITY_SPIKE = 5.0;    // %RH jump within the spike window = "shower"

const unsigned long OVERRIDE_TIME   = 15UL * 60UL * 1000UL;        // 15 min manual run
const unsigned long MAX_RUN_TIME    = 4UL * 60UL * 60UL * 1000UL;  // 4 h hard limit
const unsigned long COOLDOWN_TIME   = 15UL * 60UL * 1000UL;        // forced rest after max run
const unsigned long READ_INTERVAL   = 2000UL;                      // sensor poll
const unsigned long SPIKE_INTERVAL  = 60000UL;                     // spike comparison window
const unsigned long STATUS_INTERVAL = 10000UL;                     // verbose status line
const unsigned long DEBOUNCE_MS     = 50UL;                        // button debounce

const int SENSOR_FAIL_LIMIT = 5;     // consecutive bad reads before suspecting bad sensor

// ==========================================
// GLOBAL STATE VARIABLES
// ==========================================
Adafruit_SHT4x sht4 = Adafruit_SHT4x();

float currentHumidity = 0.0;
float currentTemp     = 0.0;
float pastHumidity    = 0.0;   // baseline for spike detection

bool fanIsRunning         = false;
bool manualOverrideActive = false;
bool cooldownActive       = false;
bool spikeDetected        = false;
bool sensorTrusted        = true;   // false after repeated read failures

int  sensorFailCount = 0;

unsigned long lastSensorRead    = 0;
unsigned long lastSpikeCheck    = 0;
unsigned long lastStatusPrint   = 0;
unsigned long overrideStartTime = 0;
unsigned long fanStartTime      = 0;
unsigned long cooldownStartTime = 0;
unsigned long lastButtonPress   = 0;

int lastButtonState = HIGH;   // for edge detection (INPUT_PULLUP idles HIGH)

const char* fanReason = "idle";

// ==========================================
// SETUP
// ==========================================
void setup() {
  // Drive the relay to a SAFE state
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);

  Serial.begin(115200);
  delay(2000);  // give the serial monitor time to attach

  Serial.println("\n\n##################################################");
  Serial.println(">>> ESP32 ALIVE - BOOT SEQUENCE STARTING <<<");
  Serial.println("##################################################\n");

  Serial.print("[SYSTEM] Relay on pin ");
  Serial.print(RELAY_PIN);
  Serial.print(" configured (");
  Serial.print(RELAY_ACTIVE_HIGH ? "active-HIGH" : "active-LOW");
  Serial.println("). Fan forced OFF.");

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.print("[SYSTEM] Button on pin ");
  Serial.print(BUTTON_PIN);
  Serial.println(" configured (INPUT_PULLUP).");

  Serial.print("[SYSTEM] Starting I2C  SDA:");
  Serial.print(SDA_PIN);
  Serial.print("  SCL:");
  Serial.println(SCL_PIN);
  Wire.begin(SDA_PIN, SCL_PIN);

  Serial.println("[SYSTEM] Searching for SHT4x sensor...");
  if (!sht4.begin(&Wire)) {
    Serial.println("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    Serial.println("CRITICAL: SHT4x SENSOR NOT DETECTED!");
    Serial.println("- Is 3V3 connected?");
    Serial.println("- Is GND connected?");
    Serial.println("- Are SDA (21) and SCL (22) swapped?");
    Serial.println("SYSTEM HALTED. Fan stays OFF. Fix wiring and reboot.");
    Serial.println("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!");
    while (1) {
      digitalWrite(RELAY_PIN, RELAY_OFF);  // keep fan safe while halted
      delay(1000);
      Serial.println("ERROR: SYSTEM HALTED. WAITING FOR SENSOR FIX.");
    }
  }
  Serial.println("[SYSTEM] SUCCESS - SHT4x found and responding.");

  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  sht4.setHeater(SHT4X_NO_HEATER);

  Serial.println("[SYSTEM] Taking environment baseline...");
  sensors_event_t humidity, temp;
  if (sht4.getEvent(&humidity, &temp)) {
    currentHumidity = humidity.relative_humidity;
    currentTemp     = temp.temperature;
    pastHumidity    = currentHumidity;
    Serial.print("[SYSTEM] Baseline -> Temp: ");
    Serial.print(currentTemp, 1);
    Serial.print(" C | Humidity: ");
    Serial.print(currentHumidity, 1);
    Serial.println(" %");
  } else {
    Serial.println("[WARN] Baseline read failed - will retry in main loop.");
  }

  Serial.println("\n##################################################");
  Serial.println(">>> BOOT COMPLETE - ENTERING MAIN LOOP <<<");
  Serial.println("##################################################\n");
}

// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  unsigned long currentMillis = millis();

  // ---------- 1. BUTTON (falling-edge + debounce) ----------
  int reading = digitalRead(BUTTON_PIN);
  if (lastButtonState == HIGH && reading == LOW) {
    if (currentMillis - lastButtonPress > DEBOUNCE_MS) {
      lastButtonPress = currentMillis;
      if (!cooldownActive) {
        manualOverrideActive = true;
        overrideStartTime = currentMillis;
        Serial.println("\n>>> TRIGGER: BUTTON PRESSED - 15-min override active <<<");
      } else {
        Serial.println("\n>>> BUTTON IGNORED: system is in COOLDOWN <<<");
      }
    }
  }
  lastButtonState = reading;

  // ---------- 2. SENSOR READ ----------
  if (currentMillis - lastSensorRead >= READ_INTERVAL) {
    lastSensorRead = currentMillis;
    sensors_event_t humidity, temp;
    if (sht4.getEvent(&humidity, &temp)) {
      currentHumidity = humidity.relative_humidity;
      currentTemp     = temp.temperature;
      if (!sensorTrusted) {
        Serial.println("[SYSTEM] Sensor recovered - readings trusted again.");
      }
      sensorTrusted   = true;
      sensorFailCount = 0;
    } else {
      sensorFailCount++;
      Serial.print("[WARN] Sensor read FAILED (");
      Serial.print(sensorFailCount);
      Serial.println(" in a row).");
      if (sensorFailCount >= SENSOR_FAIL_LIMIT && sensorTrusted) {
        sensorTrusted = false;
        Serial.println("[WARN] Sensor distrusted - humidity logic paused. "
                       "Button override still works.");
      }
    }
  }

  // ---------- 3. SPIKE DETECTION ----------
  if (currentMillis - lastSpikeCheck >= SPIKE_INTERVAL) {
    lastSpikeCheck = currentMillis;
    if (sensorTrusted) {
      float humidityDelta = currentHumidity - pastHumidity;
      if (humidityDelta >= HUMIDITY_SPIKE) {
        spikeDetected = true;
        Serial.print("\n>>> TRIGGER: SHOWER DETECTED - humidity jumped ");
        Serial.print(humidityDelta, 1);
        Serial.println("% in 60 s <<<");
      }
      pastHumidity = currentHumidity;
    }
  }

  // ---------- 4. SAFETY COOLDOWN ----------
  if (cooldownActive) {
    if (currentMillis - cooldownStartTime >= COOLDOWN_TIME) {
      cooldownActive = false;
      Serial.println("\n>>> SYSTEM: Cooldown finished - resuming monitoring <<<");
    }
  } else if (fanIsRunning && (currentMillis - fanStartTime >= MAX_RUN_TIME)) {
    cooldownActive    = true;
    cooldownStartTime = currentMillis;
    manualOverrideActive = false;
    spikeDetected     = false;
    turnFanOff();
    Serial.println("\n!!! WARNING: Fan hit 4-hour max - forced 15-min cooldown !!!");
  }

  // ---------- 5. MASTER LOGIC ----------
  if (!cooldownActive) {
    bool wantOn = manualOverrideActive
               || (sensorTrusted && (currentHumidity >= HUMIDITY_MAX || spikeDetected));

    if (wantOn && !fanIsRunning) {
      if (manualOverrideActive)      fanReason = "manual override";
      else if (spikeDetected)        fanReason = "shower spike";
      else                           fanReason = "high humidity";
      turnFanOn();
    }

    if (fanIsRunning) {
      if (manualOverrideActive && (currentMillis - overrideStartTime >= OVERRIDE_TIME)) {
        manualOverrideActive = false;
        Serial.println("\n>>> SYSTEM: Manual override expired <<<");
      }

      if (!manualOverrideActive && sensorTrusted && currentHumidity <= HUMIDITY_MIN) {
        if (spikeDetected)
          Serial.println("\n>>> SYSTEM: Humidity back to target - spike flag cleared <<<");
        spikeDetected = false;
        turnFanOff();
      }
    }
  }

  // ---------- 6. VERBOSE STATUS ----------
  if (currentMillis - lastStatusPrint >= STATUS_INTERVAL) {
    lastStatusPrint = currentMillis;
    printVerboseStatus();
  }
}

// ==========================================
// HELPER FUNCTIONS
// ==========================================
void turnFanOn() {
  digitalWrite(RELAY_PIN, RELAY_ON);
  fanIsRunning  = true;
  fanStartTime  = millis();
  Serial.print("\n[HARDWARE] =====> RELAY ON  (reason: ");
  Serial.print(fanReason);
  Serial.println(") <=====\n");
}

void turnFanOff() {
  digitalWrite(RELAY_PIN, RELAY_OFF);
  fanIsRunning = false;
  fanReason    = "idle";
  Serial.println("\n[HARDWARE] =====> RELAY OFF <=====\n");
}

void printVerboseStatus() {
  Serial.print("[DATA] Up:");
  Serial.print(millis() / 1000);
  Serial.print("s | Temp:");
  Serial.print(currentTemp, 1);
  Serial.print("C | Hum:");
  Serial.print(currentHumidity, 1);
  Serial.print("% | Baseline:");
  Serial.print(pastHumidity, 1);
  Serial.print("% | Fan:");
  Serial.print(fanIsRunning ? "ON" : "OFF");

  if (fanIsRunning) {
    Serial.print(" (");
    Serial.print(fanReason);
    Serial.print(", run:");
    Serial.print((millis() - fanStartTime) / 1000);
    Serial.print("s)");
  }

  if (!sensorTrusted) Serial.print(" | [SENSOR FAULT]");
  if (spikeDetected)  Serial.print(" | [SPIKE]");

  if (manualOverrideActive) {
    long timeLeft = (long)(OVERRIDE_TIME - (millis() - overrideStartTime)) / 1000;
    Serial.print(" | [Override:");
    Serial.print(timeLeft);
    Serial.print("s]");
  }

  if (cooldownActive) {
    long cooldownLeft = (long)(COOLDOWN_TIME - (millis() - cooldownStartTime)) / 1000;
    Serial.print(" | [COOLDOWN:");
    Serial.print(cooldownLeft);
    Serial.print("s]");
  }

  Serial.println();
}
