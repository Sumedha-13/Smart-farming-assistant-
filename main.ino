#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <string.h>

// ---------- COMPONENTS ----------
LiquidCrystal_I2C lcd(0x27, 16, 2);
Adafruit_BME280 bme;

const int soilPin = A0;
const int relayPin = 7;

// ---------- SOIL CALIBRATION ----------
// Keep your existing calibrated values.
const int DRY_RAW = 1023;
const int WET_RAW = 300;

// ---------- PUMP SETTINGS ----------
// Set this to true to reverse the moisture decision.
const bool REVERSE_PUMP_LOGIC = true;

// Keep the relay polarity setting from your current code.
// If relay ON/OFF electrical behaviour is reversed,
// change true to false, but do that as a separate test.
const bool RELAY_ACTIVE_LOW = true;

// Moisture thresholds
const int PUMP_ON_BELOW = 30;
const int PUMP_OFF_AT = 45;

// ---------- VARIABLES ----------
bool pumpOn = false;
bool bmeOK = false;
bool showEnvironment = false;

unsigned long lastRead = 0;
unsigned long lastScreen = 0;

int rawValue = 0;
int moisturePercent = 0;

float temperature = 0;
float humidity = 0;
float pressure = 0;

// ---------- RELAY CONTROL ----------
void setPump(bool turnOn) {
  pumpOn = turnOn;

  if (RELAY_ACTIVE_LOW) {
    digitalWrite(relayPin, turnOn ? LOW : HIGH);
  } else {
    digitalWrite(relayPin, turnOn ? HIGH : LOW);
  }
}

// ---------- SOIL SENSOR ----------
int readSoil() {
  long total = 0;

  for (int i = 0; i < 10; i++) {
    total += analogRead(soilPin);
    delay(5);
  }

  return total / 10;
}

// ---------- LCD LINE ----------
void printLine(byte row, const char *message) {
  lcd.setCursor(0, row);
  lcd.print(message);

  int length = strlen(message);

  while (length < 16) {
    lcd.print(' ');
    length++;
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(9600);
  Wire.begin();

  pinMode(relayPin, OUTPUT);

  // Pump OFF during startup.
  digitalWrite(relayPin, RELAY_ACTIVE_LOW ? HIGH : LOW);

  lcd.init();
  lcd.backlight();

  printLine(0, "SMART FARMING");
  printLine(1, "Starting...");
  delay(1500);

  bmeOK = bme.begin(0x76);

  if (!bmeOK) {
    bmeOK = bme.begin(0x77);
  }

  if (!bmeOK) {
    Serial.println("BME280 not detected");
  }

  lcd.clear();
}

// ---------- MAIN LOOP ----------
void loop() {
  unsigned long now = millis();

  // Read sensors once per second.
  if (now - lastRead >= 1000) {
    lastRead = now;

    rawValue = readSoil();

    bool calibrationOK = (DRY_RAW - WET_RAW >= 100);

    if (calibrationOK) {
      moisturePercent = map(
        rawValue,
        DRY_RAW,
        WET_RAW,
        0,
        100
      );

      moisturePercent = constrain(
        moisturePercent, 0, 100
      );

      // Determine whether the soil needs watering.
      bool soilNeedsWater;

      if (REVERSE_PUMP_LOGIC) {
        // Reversed decision:
        // Higher displayed moisture -> request pump ON.
        soilNeedsWater =
          (moisturePercent >= PUMP_OFF_AT);
      } else {
        // Normal decision:
        // Lower displayed moisture -> request pump ON.
        soilNeedsWater =
          (moisturePercent < PUMP_ON_BELOW);
      }

      // Control pump.
      if (soilNeedsWater) {
        setPump(true);
      } else {
        setPump(false);
      }

    } else {
      // Invalid calibration: keep pump OFF.
      setPump(false);
      moisturePercent = 0;
    }

    // Read BME280.
    if (bmeOK) {
      temperature = bme.readTemperature();
      humidity = bme.readHumidity();
      pressure = bme.readPressure() / 100.0F;
    }

    // Serial Monitor output.
    Serial.print("Raw: ");
    Serial.print(rawValue);

    Serial.print(" | Moisture: ");
    Serial.print(moisturePercent);
    Serial.print("%");

    Serial.print(" | Pump command: ");
    Serial.print(pumpOn ? "ON" : "OFF");

    if (bmeOK) {
      Serial.print(" | Temp: ");
      Serial.print(temperature, 1);

      Serial.print(" C | Humidity: ");
      Serial.print(humidity, 1);
      Serial.print("%");
    }

    Serial.println();
  }

  // Change LCD screen every 3 seconds.
  if (now - lastScreen >= 3000) {
    lastScreen = now;
    showEnvironment = !showEnvironment;
  }

  // ---------- LCD SCREEN 1 ----------
  if (!showEnvironment) {
    char line1[17];
    char line2[17];

    if (DRY_RAW - WET_RAW < 100) {
      printLine(0, "Check calibration");
      printLine(1, "Pump OFF");
    } else {
      snprintf(
        line1,
        sizeof(line1),
        "Soil:%3d%% R:%4d",
        moisturePercent,
        rawValue
      );

      snprintf(
        line2,
        sizeof(line2),
        "Pump: %s",
        pumpOn ? "ON" : "OFF"
      );

      printLine(0, line1);
      printLine(1, line2);
    }
  }

  // ---------- LCD SCREEN 2 ----------
  else {
    if (bmeOK) {
      lcd.setCursor(0, 0);
      lcd.print("Temp: ");
      lcd.print(temperature, 1);
      lcd.write((byte)223);
      lcd.print("C   ");

      lcd.setCursor(0, 1);
      lcd.print("Humidity: ");
      lcd.print(humidity, 1);
      lcd.print("%   ");
    } else {
      printLine(0, "BME280 Error");
      printLine(1, "Check wiring");
    }
  }
}
