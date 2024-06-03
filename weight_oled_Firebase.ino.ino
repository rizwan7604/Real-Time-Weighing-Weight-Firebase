#include <HX711_ADC.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#if defined(ESP8266) || defined(ESP32)
#include <EEPROM.h>
#include <WiFi.h>
#include <FirebaseESP32.h> 
#endif

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Pins
const int HX711_dout = 23; // MCU > HX711 dout pin D23
const int HX711_sck = 19; // MCU > HX711 sck pin D22

// HX711 constructor
HX711_ADC LoadCell(HX711_dout, HX711_sck);

// WiFi credentials
const char* ssid = "V2025";
const char* password = "123456789";

// Firebase credentials
#define FIREBASE_HOST "https://real-time-weight-28320-default-rtdb.asia-southeast1.firebasedatabase.app"
#define FIREBASE_AUTH "4ZNMnLQU5SoePhuWvkCmn9RrlAbgXokzHRjMWh2f"

// Firebase objects
FirebaseData firebaseData;
FirebaseAuth firebaseAuth;
FirebaseConfig firebaseConfig;

const int calVal_eepromAdress = 0;
unsigned long t = 0;

void setup() {
  Serial.begin(57600); delay(10);
  Serial.println();
  Serial.println("Starting...");

  // Initialize OLED
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("Connected to Wi-Fi");

  // Configure Firebase
  firebaseConfig.host = FIREBASE_HOST;
  firebaseConfig.signer.tokens.legacy_token = FIREBASE_AUTH;

  // Initialize Firebase
  Firebase.begin(&firebaseConfig, &firebaseAuth);
  Firebase.reconnectWiFi(true);

  float calibrationValue; // calibration value
  calibrationValue = 216.83; // uncomment this if you want to set this value in the sketch
#if defined(ESP8266) || defined(ESP32)
  //EEPROM.begin(512); // uncomment this if you use ESP8266 and want to fetch this value from eeprom
#endif
  //EEPROM.get(calVal_eepromAdress, calibrationValue); // uncomment this if you want to fetch this value from eeprom

  LoadCell.begin();
  //LoadCell.setReverseOutput();
  unsigned long stabilizingtime = 2000; // tare precision can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; // set this to false if you don't want tare to be performed in the next step
  LoadCell.start(stabilizingtime, _tare);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 wiring and pin designations");
  } else {
    LoadCell.setCalFactor(calibrationValue); // set calibration factor (float)
    Serial.println("Startup is complete");
  }
  while (!LoadCell.update());
  Serial.print("Calibration value: ");
  Serial.println(LoadCell.getCalFactor());
  Serial.print("HX711 measured conversion time ms: ");
  Serial.println(LoadCell.getConversionTime());
  Serial.print("HX711 measured sampling rate HZ: ");
  Serial.println(LoadCell.getSPS());
  Serial.print("HX711 measured settling time ms: ");
  Serial.println(LoadCell.getSettlingTime());
  Serial.println("Note that the settling time may increase significantly if you use delay() in your sketch!");
  if (LoadCell.getSPS() < 7) {
    Serial.println("!!Sampling rate is lower than specification, check MCU>HX711 wiring and pin designations");
  } else if (LoadCell.getSPS() > 100) {
    Serial.println("!!Sampling rate is higher than specification, check MCU>HX711 wiring and pin designations");
  }

  // Clear the display buffer.
  display.clearDisplay();
}

void loop() {
  static boolean newDataReady = false;
  const unsigned long serialPrintInterval = 1000; // Interval for printing to Serial
  const unsigned long firebaseUpdateInterval = 2000; // Interval for sending data to Firebase
  const unsigned long oledUpdateInterval = 1000; // Interval for updating OLED display

  unsigned long currentMillis = millis();

  // Check for new data/start next conversion
  if (LoadCell.update()) {
    newDataReady = true;
  }

  // Get smoothed value from the dataset
  if (newDataReady && (currentMillis - t >= serialPrintInterval)) {
    float weight = LoadCell.getData();
    Serial.print("Load_cell output val: ");
    Serial.print(weight); // Print weight value
    Serial.println(" g"); // Print unit (gram)
    
    String weightWithUnit = String(weight) + " g";

    // Display weight value on OLED
    display.clearDisplay();
    display.setTextSize(2);      // Normal 1:1 pixel scale
    display.setTextColor(SSD1306_WHITE); // Draw white text
    display.setCursor(0,0);     // Start at top-left corner
    display.println("Weight:");
    display.println(weightWithUnit);
    display.display(); // Display text

    // Send weight value to Firebase
    if (Firebase.setString(firebaseData, "/weight", weightWithUnit)) {
      Serial.println("Data sent to Firebase successfully");
    } else {
      Serial.print("Failed to send data to Firebase: ");
      Serial.println(firebaseData.errorReason());
    }

    t = currentMillis; // Update the last sent time
  }

  // Receive command from serial terminal, send 't' to initiate tare operation
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') LoadCell.tareNoDelay();
  }

  // Check if last tare operation is complete
  if (LoadCell.getTareStatus()) {
    Serial.println("Tare complete");
  }
}
