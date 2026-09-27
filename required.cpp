#include <DHT.h>

#define MQ4_PIN      A0
#define MQ2_PIN      A1
#define SOIL_PIN     A2
#define DHTPIN       2
#define DHTTYPE      DHT11
#define FLAME_PIN    3

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(115200);
  Serial1.begin(9600); // Hardware Serial1: TX1 = Pin 18, RX1 = Pin 19
  pinMode(FLAME_PIN, INPUT);
  dht.begin();
  Serial.println("Arduino Mega Sensor Acquisition Online");
}

void loop() {
  int mq4Raw = analogRead(MQ4_PIN);
  int mq2Raw = analogRead(MQ2_PIN);
  int soilRaw = analogRead(SOIL_PIN);
  int flameVal = (digitalRead(FLAME_PIN) == LOW) ? 1 : 0; // Active LOW flame sensor

  float t = dht.readTemperature();
  float h = dht.readHumidity();

  // If DHT fails to read, send -1.0 so dashboard flags wire status instead of false 42C
  if (isnan(t) || isnan(h)) {
    t = -1.0;
    h = -1.0;
  }

  // Comma-separated telemetry: MQ2,MQ4,Soil,Temp,Humidity,Flame
  String packet = String(mq2Raw) + "," +
                  String(mq4Raw) + "," +
                  String(soilRaw) + "," +
                  String(t, 1) + "," +
                  String((int)h) + "," +
                  String(flameVal);

  Serial1.println(packet);
  Serial.println("Mega Telemetry: " + packet);

  delay(1000);
}