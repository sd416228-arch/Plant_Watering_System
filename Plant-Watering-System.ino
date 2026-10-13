
#define SOIL_SENSOR_PIN 34
#define RELAY_PIN 26

// Adjust this threshold after testing your sensor
const int DRY_THRESHOLD = 2500;

// Most relay modules are active LOW
const int PUMP_ON = LOW;
const int PUMP_OFF = HIGH;

void setup() {
  Serial.begin(115200);

  pinMode(SOIL_SENSOR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);

  digitalWrite(RELAY_PIN, PUMP_OFF);

  Serial.println("ESP32 Plant Watering System Started");
}

void loop() {
  int moisture = analogRead(SOIL_SENSOR_PIN);

  Serial.print("Soil Moisture: ");
  Serial.println(moisture);

  if (moisture > DRY_THRESHOLD) {
    digitalWrite(RELAY_PIN, PUMP_ON);
    Serial.println("Soil is dry - Pump ON");
  } else {
    digitalWrite(RELAY_PIN, PUMP_OFF);
    Serial.println("Soil is moist - Pump OFF");
  }

  delay(2000);
}