#include <Arduino.h>
// Libraries for LoRa
#include <SPI.h>
#include <LoRa.h>

// Define the pins used by the LoRa transceiver module
#define ss 5
#define rst 14
#define dio0 4

// Define the LoRa band (frequency)
#define BAND 433E6 // 433E6 for Asia, 866E6 for Europe, 915E6 for North America

// Packet counter
int readingID = 0;
int counter = 0;
const int locationID = 1; // location identifier

// Define sensor pins
const int turbiditySensorPin = 34;  // Placeholder for water turbidity sensor (using water level sensor pin)
const int flowSensorPin = 33;       // Water flow sensor pin
const int trigPin = 25;             // Ultrasonic sensor trig pin
const int echoPin = 26;             // Ultrasonic sensor echo pin
const int ledPin = 2;               // Built-in LED pin on ESP32

// Variables for sensor readings
int turbidityValue = 0;
float turbidity = 0.0;
float distance = 0.0;
//int headwaterIncident = 0; // Variable to indicate headwater incident

//define sound speed in cm/uS
#define SOUND_SPEED 0.034

// Flow sensor variables
float calibrationFactor = 4.5;
volatile byte pulseCount = 0;
float flowRate = 0.0;
unsigned long previousMillis = 0;


// Timer variables(may need adjustment)
unsigned long lastTime = 0;
unsigned long timerDelay = 15000;
unsigned long loraDelay = 15000;

// Initialize LoRa module
void startLoRA() {
    LoRa.setPins(ss, rst, dio0); // Setup LoRa transceiver module

    while (!LoRa.begin(BAND) && counter < 10) {
        Serial.print(".");
        counter++;
        delay(500);
    }
    if (counter == 10) {
        Serial.println("Starting LoRa failed!");
    } else {
        Serial.println("LoRa Initialization OK!");
    }
    delay(2000);
}

void sendReadings() {
  String data = constructDataMessage(readingID, locationID, turbidity, flowRate, distance);
  uint8_t checksum = calculateChecksum(data);
  String LoRaMessage = data + "/" + String(checksum);

  if (sendLoRaPacket(LoRaMessage)) {
      Serial.print("Successfully sent packet: ");
  } else {
      Serial.print("Failed to send packet: ");
  }
  Serial.println(readingID);
  Serial.println(LoRaMessage);  
  Serial.println("-----------------------------------------");
  readingID++;
}


float readUltrasonicDistance(int trigPin, int echoPin) {
  // Initialize variables
  unsigned long duration = 0;
  float distanceCm = 0.0;
  
  // Clear the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  // Send a 10 microsecond pulse to the trigPin
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Read the echoPin, return the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  
  // Calculate the distance in centimeters
  distanceCm = duration * SOUND_SPEED / 2.0;
  
  return distanceCm;
}

void updateFlowRate() {
  
  // Capture and reset pulse count for the past second
  noInterrupts();
  byte pulse1Sec = pulseCount;
  pulseCount = 0;
  interrupts();

  // Calculate flow rate based on pulses per second and calibration factor
  if (millis() != previousMillis) { // Prevent division by zero
      flowRate = ((1000.0 / (millis() - previousMillis)) * pulse1Sec) / calibrationFactor;
      previousMillis = millis();
  }
}


void pulseCounter() {
  pulseCount++;
}



uint8_t calculateChecksum(String data) {
  uint8_t checksum = 0;
  for (int i = 0; i < data.length(); i++) {
    checksum += data[i];
  }
  return checksum;
}

String constructDataMessage(int id, int location, float turbidity, float flow, float dist) {
    return String(id) + "/" + String(location) + "/" + String(turbidity, 2) + "/" + String(flow, 2) + "/" + String(dist, 2);
}

bool sendLoRaPacket(String message) {
    LoRa.beginPacket();
    LoRa.print(message);
    bool success = LoRa.endPacket(); // Check if packet transmission was successful
    if (success) {
        Serial.println("Packet sent successfully!");
        return true;
    } else {
        Serial.println("Failed to send packet!");
        return false;
    }
}


void readSensors() {
    turbidityValue = analogRead(turbiditySensorPin);
    // Assuming linear mapping: 0-4095 raw value to 0-1000 NTU
    turbidity = map(turbidityValue, 0, 4095, 0, 1000);
    distance = readUltrasonicDistance(trigPin, echoPin);
    updateFlowRate();
}

void printSensorValues() {
    Serial.print("Turbidity: ");
    Serial.print(turbidity);
    Serial.println(" NTU");
    Serial.print("Flow rate: ");
    Serial.print(flowRate);
    Serial.println(" L/min");
    Serial.print("Distance (cm): ");
    Serial.println(distance);
    
}

void setup() {
  Serial.begin(9600);
  pinMode(ledPin, OUTPUT);          // LED pin as output
  pinMode(turbiditySensorPin, INPUT);  // Turbidity sensor pin as input
  pinMode(flowSensorPin, INPUT);       // Flow sensor pin as input
  pinMode(trigPin, OUTPUT);            // Ultrasonic sensor trigger pin as output
  pinMode(echoPin, INPUT);             // Ultrasonic sensor echo pin as input
  // Attach interrupt for flow sensor
  attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, FALLING);
  startLoRA();
}

void loop() {
  readSensors();
  printSensorValues();

  if((millis() - lastTime)>loraDelay){
    sendReadings();
    lastTime = millis();
  }
  
  delay(timerDelay);  // Adjust delay as needed
}


