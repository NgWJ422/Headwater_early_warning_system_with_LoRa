#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Define the pins used by the LoRa transceiver module
#define ss 5
#define rst 14
#define dio0 2

#define BAND 433E6  // 433E6 for Asia, 866E6 for Europe, 915E6 for North America

// Wi-Fi credentials
// const char *SSID = "1Oz6-2.4G";
// const char *PASSWORD = "1_Oz6@915";
// const char *SSID = "Redmi Note 9";
// const char *PASSWORD = "vucq4581";
const char *SSID = "vivo 1906";
const char *PASSWORD = "720e325197bd";


const char *SERVER = "api.thingspeak.com";

// ThingSpeak API key
const String API_KEY = "938ISEH1ZBCCX0KR";

// WiFiClient instance
WiFiClient client;

// Variables to store LoRa data
int counter = 0;
int readingID;
int locationID;
float turbidity = 0;
float flowRate = 0;
float distance = 0;
bool headwaterIncident = false;
int rssi;
bool newpacket = false;
int badpacket = 0;
int error = 0;
const int buzzerPin = 13; // GPIO pin connected to the buzzer (PWM capable)
int volume = 20; // Default volume (0 to 255)
int frequency = 1000; // Default frequency in Hz
int screen = 0;

// Thresholds for headwater incident detection (adjust according to sensor specifications)
const float turbidityThreshold = 500.0;  // Example threshold for turbidity
const float flowThreshold = 1.0;         // Example threshold for flow rate
const float distanceThreshold = 10.0;    // Example threshold for distance/water level

// Interval for recheck wifi connection
unsigned long checklasttime=0;
unsigned long checkwifitime=10000;
// Interval for the length of upload to ThingSpeak
unsigned long lastTime = 0;
unsigned long thingspeakDelay = 15000;
//Interval for display update
unsigned long displaylasttime=0;
unsigned long displayinterval = 2000;
//Interval for reset
unsigned long resetlasttime=0;
unsigned long resetinterval = 300000;
//Interval for receive lora packet
unsigned long loralasttime= -10000;
unsigned long lorainterval = 10000;


// Function prototypes
void setupLoRa();
void receiveLoRaPacket();
void processLoRaData(String LoRaData);
void printReceivedData();
void checkHeadwaterIncident();
uint8_t calculateChecksum(String data);
void sendToThingSpeak();
void connectWifi();
void checkWifiConnection();
void updateDisplay();
void ringbuzzer();
unsigned long volumeToDuration(int volume);


void setupLoRa() {
  // Setup LoRa transceiver module
  LoRa.setPins(ss, rst, dio0);

  while (!LoRa.begin(BAND) && counter < 20) {
    Serial.print(".");
    counter++;
    delay(2000);
  }
  if (counter == 20) {
    Serial.println("Starting LoRa failed!");
  } else {
    Serial.println("LoRa Initialization OK!");
  }
  delay(2000);
}

void connectWifi() {
  // Connect to Wi-Fi network with SSID and password
  Serial.print("Connecting to ");
  Serial.println(SSID);
  WiFi.begin(SSID, PASSWORD);
  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 30000) {
    delay(2000);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    // Print local IP address
    Serial.println("");
    Serial.println("WiFi connected.");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("Failed to connect to WiFi.");
  }
}

void checkWifiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    connectWifi();
  }
}

void receiveLoRaPacket() {
  int packetSize = LoRa.parsePacket();
  if (packetSize && (millis() - loralasttime >= lorainterval)) {
    display.clearDisplay();
    display.setCursor(0, 10);
    display.print("Lora packet received");
    display.display();
    Serial.print("LoRa packet received: ");
    String LoRaData = LoRa.readString();
    Serial.println(LoRaData);
    delay(1000);
    // Get RSSI of the received packet
    rssi = LoRa.packetRssi();

    processLoRaData(LoRaData);
  }
}

void processLoRaData(String LoRaData) {
  int pos1 = LoRaData.indexOf('/');
  int pos2 = LoRaData.indexOf('/', pos1 + 1);
  int pos3 = LoRaData.indexOf('/', pos2 + 1);
  int pos4 = LoRaData.indexOf('/', pos3 + 1);
  int pos5 = LoRaData.lastIndexOf('/');  // Last '/' position for checksum

  if (pos1 != -1 && pos2 != -1 && pos3 != -1 && pos4 != -1 && pos5 != -1) {
    readingID = LoRaData.substring(0, pos1).toInt();
    locationID = LoRaData.substring(pos1 + 1, pos2).toInt();
    turbidity = LoRaData.substring(pos2 + 1, pos3).toFloat();
    flowRate = LoRaData.substring(pos3 + 1, pos4).toFloat();
    distance = LoRaData.substring(pos4 + 1, pos5).toFloat();

    // Extract checksum and data without checksum
    String dataWithoutChecksum = LoRaData.substring(0, pos5);
    uint8_t receivedChecksum = LoRaData.substring(pos5 + 1).toInt();

    // Calculate checksum for received data
    uint8_t calculatedChecksum = calculateChecksum(dataWithoutChecksum);

    if (receivedChecksum == calculatedChecksum) {
      checkHeadwaterIncident();
      printReceivedData();
      newpacket = true;
      loralasttime = millis();
    } else {
      badpacket++;
      newpacket = false;
      display.clearDisplay();
      display.setCursor(0, 10);
      display.print("error packet");
      display.display();
      Serial.println("Checksum mismatch! Data corrupted.");
      Serial.println("-----------------------------------");
      delay(3000);
    }
  } else {
    badpacket++;
    newpacket = false;
    display.clearDisplay();
    display.setCursor(0, 10);
    display.print("error packet");
    display.display();
    Serial.println("Error parsing LoRaData");
    Serial.println("-----------------------------------");
    delay(3000);
  }
}

void printReceivedData() {
  Serial.println("Checksum verified. Data received:");
  Serial.print("Reading ID: ");
  Serial.println(readingID);
  Serial.print("Location ID: ");
  Serial.println(locationID);
  Serial.print("Turbidity Value: ");
  Serial.println(turbidity);
  Serial.print("Flow Rate: ");
  Serial.println(flowRate);
  Serial.print("Distance/Water level: ");
  Serial.println(distance);
  Serial.print("RSSI: ");
  Serial.println(rssi);
  Serial.print("Headwater Incident: ");
  Serial.println(headwaterIncident);
  Serial.println("-----------------------------------");
}

void checkHeadwaterIncident() {
  // Example condition for headwater incident detection
  if (turbidity < turbidityThreshold && flowRate > flowThreshold && distance < distanceThreshold) {
    headwaterIncident = true;
    Serial.println("Headwater incident detected! Warning activated.");
  } else {
    headwaterIncident = false;
  }
}

uint8_t calculateChecksum(String data) {
  uint8_t checksum = 0;
  for (int i = 0; i < data.length(); i++) {
    checksum += data[i];
  }
  error++;
  //Introduce a 1 in 10 chance to increment the checksum for testing
  if (error == 10) {
    checksum += 1;
    error=0;
  }
  
  return checksum;
}

void sendToThingSpeak() {

  // Check Wi-Fi connection before sending data
  checkWifiConnection();

  //make a HTTP request to ThingSpeak
  if (client.connect(SERVER, 80)) {
    String postStr = API_KEY;
    postStr += "&field1=";
    postStr += String(readingID);
    postStr += "&field2=";
    postStr += String(turbidity);
    postStr += "&field3=";
    postStr += String(flowRate);
    postStr += "&field4=";
    postStr += String(distance);
    postStr += "&field5=";
    postStr += String(headwaterIncident);
    postStr += "&field6=";
    postStr += String(rssi);
    postStr += "&field7=";
    postStr += String(badpacket);
    postStr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\n");
    client.print("Host: api.thingspeak.com\n");
    client.print("Connection: close\n");
    client.print("X-THINGSPEAKAPIKEY: " + API_KEY + "\n");
    client.print("Content-Type: application/x-www-form-urlencoded\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\n\n");
    client.print(postStr);

    Serial.println("Data sent to ThingSpeak: ");
    Serial.println(postStr);

    // Read the HTTP response
    while (client.connected()) {
      String line = client.readStringUntil('\n');
      if (line == "\r") {
        Serial.println("Headers received");
        break;
      }
    }
    String response = client.readStringUntil('\n');
    Serial.print("Response: ");
    Serial.println(response);
    Serial.println("...............................................................");

    if (response == "0") {
      Serial.println("Error: Data not accepted");
    }
    display.clearDisplay();
    display.setCursor(0, 10);
    display.print("success");
    display.display();
    badpacket = 0;
    delay(1000);
    client.stop();
  } else {
    display.clearDisplay();
    display.setCursor(0, 10);
    display.print("fail");
    display.display();
    Serial.println("Connection to ThingSpeak failed.");
    delay(1000);
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setCursor(0, 10);

  switch (screen) {
    case 0:
      if (headwaterIncident) {
        display.print("Headwater incident  detected");
      } else {
        display.print("Safe, No  headwater incident");
      }
      break;
      
    case 1:
      display.print("Turbidity (NTU) =   ");
      display.print(turbidity);
      break;
      
    case 2:
      display.print("Flow Rate (L/min) = ");
      display.print(flowRate);
      break;
      
    case 3:
      display.print("Water     level(cm)=");
      display.print(distance);
      break;
  }

  display.display();

  // Increment screen counter and loop back to 0 after the last screen
  screen = (screen + 1) % 4;
}


unsigned long volumeToDuration(int volume) {
  return map(volume, 0, 255, 0, 2000); // Map volume to duration in milliseconds
}

void ringbuzzer(){
  // Buzz with specific frequency and duration based on volume
  unsigned long duration = volumeToDuration(volume);
  if (headwaterIncident) {
    tone(buzzerPin, frequency, duration);
  } else {
    noTone(buzzerPin);
  }
}


void setup() {
  Serial.begin(115200);
  pinMode(buzzerPin, OUTPUT); // Set the buzzer pin as an output
  setupLoRa();
  connectWifi();

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(1000); // Pause for 1 seconds
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  
}

void loop() {
  if ((millis() - checklasttime) > checkwifitime) {
    checkWifiConnection();
    checklasttime = millis();
  }
  receiveLoRaPacket();

  ringbuzzer();

  if ((millis() - lastTime) > thingspeakDelay && newpacket == true) {
    Serial.println("Sending data to Thingspeak");
    display.clearDisplay();
    display.setCursor(0, 10);
    display.print("uploading to Thingspeak");
    display.display();
    sendToThingSpeak();
    lastTime = millis();
    newpacket = false;
    screen = 0;
  }

  if ((millis() - displaylasttime) > displayinterval) {
    updateDisplay();
    displaylasttime = millis();
  }

  if (millis() - resetlasttime >= resetinterval) {
    Serial.println("Resetting ESP32...");
    display.clearDisplay();
    display.setCursor(0, 10);
    display.print("resetting......");
    display.display();
    esp_restart(); // Restart the ESP32
    resetlasttime = millis();
  }
}
