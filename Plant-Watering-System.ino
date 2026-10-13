#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>
#include <DHT.h>

// ---------------- Pin configuration ----------------
#define ENTRANCE_TRIG_PIN 2
#define ENTRANCE_ECHO_PIN 3

#define SLOT1_TRIG_PIN 4
#define SLOT1_ECHO_PIN 5

#define SLOT2_TRIG_PIN 6
#define SLOT2_ECHO_PIN 7

// Added connection for the humidity sensor.
#define DHT_PIN 8
#define DHT_TYPE DHT11  // Change to DHT22 if that is your sensor.

#define GATE_SERVO_PIN 9
#define SECOND_SERVO_PIN 10

#define MQ2_ANALOG_PIN A0

// Both LCDs use SDA=A4 and SCL=A5, but must have different addresses.
LiquidCrystal_I2C parkingLCD(0x27, 16, 2);
LiquidCrystal_I2C environmentLCD(0x26, 16, 2);

Servo gateServo;
// Servo 2 has no requested action yet, so it is intentionally not attached.

DHT dht(DHT_PIN, DHT_TYPE);

// ---------------- System settings ----------------
const float DETECTION_DISTANCE_CM = 10.0;

const int GATE_CLOSED_ANGLE = 0;
const int GATE_OPEN_ANGLE = 90;

const unsigned long GATE_MINIMUM_OPEN_TIME = 5000;
const unsigned long DISPLAY_UPDATE_INTERVAL = 500;
const unsigned long ENVIRONMENT_READ_INTERVAL = 2000;

bool gateOpen = false;
unsigned long gateOpenedAt = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long lastEnvironmentRead = 0;

int gasRawValue = 0;
float humidityValue = NAN;

// ---------------- Display helpers ----------------
void printLCDLine(LiquidCrystal_I2C &lcd, byte row, const String &text) {
  lcd.setCursor(0, row);

  for (byte column = 0; column < 16; column++) {
    if (column < text.length()) {
      lcd.print(text.charAt(column));
    } else {
      lcd.print(' ');
    }
  }
}

// ---------------- Ultrasonic helpers ----------------
float readDistanceCM(byte trigPin, byte echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  unsigned long duration = pulseIn(echoPin, HIGH, 30000);

  if (duration == 0) {
    return -1.0;
  }

  return duration * 0.0343 / 2.0;
}

bool objectDetected(float distance) {
  return distance > 0 && distance <= DETECTION_DISTANCE_CM;
}

// ---------------- MQ-2 helper ----------------
int readAveragedGasValue() {
  long total = 0;

  for (byte sample = 0; sample < 10; sample++) {
    total += analogRead(MQ2_ANALOG_PIN);
    delay(2);
  }

  return total / 10;
}

// ---------------- Gate control ----------------
void controlGate(bool entranceDetected, bool parkingFull) {
  // Open only if a car is detected and at least one slot is free.
  if (!gateOpen && entranceDetected && !parkingFull) {
    gateServo.write(GATE_OPEN_ANGLE);
    gateOpen = true;
    gateOpenedAt = millis();

    Serial.println("Gate opening to 90 degrees");
  }

  // Keep the gate open for at least five seconds and close after the car leaves.
  if (gateOpen &&
      millis() - gateOpenedAt >= GATE_MINIMUM_OPEN_TIME &&
      !entranceDetected) {
    gateServo.write(GATE_CLOSED_ANGLE);
    gateOpen = false;

    Serial.println("Gate closing to 0 degrees");
  }
}

// ---------------- LCD updates ----------------
void updateParkingLCD(bool slot1Occupied, bool slot2Occupied, int freeSlots) {
  String line1 =
    "S1:" + String(slot1Occupied ? "FULL" : "FREE") +
    " S2:" + String(slot2Occupied ? "FULL" : "FREE");

  String line2 =
    "Free:" + String(freeSlots) +
    " G:" + String(gateOpen ? "OPEN" : "CLOSED");

  printLCDLine(parkingLCD, 0, line1);
  printLCDLine(parkingLCD, 1, line2);
}

void updateEnvironmentLCD() {
  printLCDLine(environmentLCD, 0, "Gas raw: " + String(gasRawValue));

  if (isnan(humidityValue)) {
    printLCDLine(environmentLCD, 1, "Humidity: ERROR");
  } else {
    printLCDLine(
      environmentLCD,
      1,
      "Humidity:" + String(humidityValue, 1) + "%"
    );
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(ENTRANCE_TRIG_PIN, OUTPUT);
  pinMode(ENTRANCE_ECHO_PIN, INPUT);

  pinMode(SLOT1_TRIG_PIN, OUTPUT);
  pinMode(SLOT1_ECHO_PIN, INPUT);

  pinMode(SLOT2_TRIG_PIN, OUTPUT);
  pinMode(SLOT2_ECHO_PIN, INPUT);

  digitalWrite(ENTRANCE_TRIG_PIN, LOW);
  digitalWrite(SLOT1_TRIG_PIN, LOW);
  digitalWrite(SLOT2_TRIG_PIN, LOW);

  gateServo.attach(GATE_SERVO_PIN);
  gateServo.write(GATE_CLOSED_ANGLE);

  dht.begin();

  Wire.begin();

  parkingLCD.init();
  parkingLCD.backlight();
  parkingLCD.clear();

  environmentLCD.init();
  environmentLCD.backlight();
  environmentLCD.clear();

  printLCDLine(parkingLCD, 0, "SMART PARKING");
  printLCDLine(parkingLCD, 1, "Starting...");

  printLCDLine(environmentLCD, 0, "ENVIRONMENT");
  printLCDLine(environmentLCD, 1, "Starting...");

  delay(1500);
}

void loop() {
  // Trigger the ultrasonic sensors separately to reduce cross-talk.
  float entranceDistance =
    readDistanceCM(ENTRANCE_TRIG_PIN, ENTRANCE_ECHO_PIN);

  delay(60);

  float slot1Distance =
    readDistanceCM(SLOT1_TRIG_PIN, SLOT1_ECHO_PIN);

  delay(60);

  float slot2Distance =
    readDistanceCM(SLOT2_TRIG_PIN, SLOT2_ECHO_PIN);

  bool entranceDetected = objectDetected(entranceDistance);
  bool slot1Occupied = objectDetected(slot1Distance);
  bool slot2Occupied = objectDetected(slot2Distance);

  int occupiedSlots = slot1Occupied + slot2Occupied;
  int freeSlots = 2 - occupiedSlots;
  bool parkingFull = freeSlots == 0;

  controlGate(entranceDetected, parkingFull);

  // DHT sensors should not be read continuously; update every two seconds.
  if (millis() - lastEnvironmentRead >= ENVIRONMENT_READ_INTERVAL) {
    lastEnvironmentRead = millis();

    gasRawValue = readAveragedGasValue();

    float newHumidity = dht.readHumidity();
    if (!isnan(newHumidity)) {
      humidityValue = newHumidity;
    }
  }

  if (millis() - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    lastDisplayUpdate = millis();

    updateParkingLCD(slot1Occupied, slot2Occupied, freeSlots);
    updateEnvironmentLCD();
  }

  Serial.print("Entrance: ");
  Serial.print(entranceDistance);

  Serial.print(" cm | Slot 1: ");
  Serial.print(slot1Occupied ? "FULL" : "FREE");

  Serial.print(" | Slot 2: ");
  Serial.print(slot2Occupied ? "FULL" : "FREE");

  Serial.print(" | Gas raw: ");
  Serial.print(gasRawValue);

  Serial.print(" | Humidity: ");
  if (isnan(humidityValue)) {
    Serial.print("ERROR");
  } else {
    Serial.print(humidityValue, 1);
    Serial.print('%');
  }

  Serial.print(" | Gate: ");
  Serial.println(gateOpen ? "OPEN" : "CLOSED");

  delay(100);
}