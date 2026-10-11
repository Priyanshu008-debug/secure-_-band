#include <BluetoothSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <QMC5883LCompass.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run menuconfig to enable it.
#endif

BluetoothSerial SerialBT;

// --- OLED Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Compass & Pins ---
QMC5883LCompass compass;
#define BUZZER_PIN 25

// --- Communication Mode Tracking ---
enum CommMode { MODE_SERIAL_UNO, MODE_BLUETOOTH };
CommMode currentMode = MODE_SERIAL_UNO;

unsigned long lastSerialPacketTime = 0;
const unsigned long FALLBACK_TIMEOUT = 3000; // Switch to Bluetooth if no serial data for 3 seconds

bool dataReceived = false;
String activeSource = "Initializing...";

void setup() {
  // 1. Initialize System Serial (Debug)
  Serial.begin(115200);
  
  // 2. Initialize Hardware Serial for Arduino Uno (Using Serial2: RX=16, TX=17)
  Serial2.begin(9600, SERIAL_8N1, 16, 17);

  // 3. Initialize Buzzer & I2C/OLED/Compass
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(21, 22);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  compass.init();

  // 4. Initialize Bluetooth (ready as fallback)
  SerialBT.begin("ESP32_BaseSender", true); // Master mode or adjust as needed

  display.setCursor(0, 0);
  display.println(F("Dual-Mode Booting..."));
  display.display();
  delay(1000);
}

void loop() {
  // Read local compass heading
  compass.read();
  int baseHeading = compass.getAzimuth();

  String incomingData = "";

  // --- STEP 1: Check Primary Source (Arduino Uno via Hardware Serial2) ---
  if (Serial2.available() > 0) {
    incomingData = Serial2.readStringUntil('\n');
    incomingData.trim();
    if (incomingData.length() > 0) {
      lastSerialPacketTime = millis();
      currentMode = MODE_SERIAL_UNO;
      activeSource = "Arduino Uno (UART)";
      dataReceived = true;
    }
  }

  // --- STEP 2: Fallback Check (If Arduino Uno data stops, switch to Bluetooth) ---
  if (millis() - lastSerialPacketTime > FALLBACK_TIMEOUT) {
    currentMode = MODE_BLUETOOTH;
    activeSource = "Bluetooth Fallback";

    // Try reading from Bluetooth if connected
    if (SerialBT.connected() && SerialBT.available()) {
      incomingData = SerialBT.readStringUntil('\n');
      incomingData.trim();
      if (incomingData.length() > 0) {
        dataReceived = true;
      }
    } else {
      // Auto-reconnect or search for BT receiver if in fallback mode
      static unsigned long lastBTAttempt = 0;
      if (millis() - lastBTAttempt > 4000) {
        lastBTAttempt = millis();
        SerialBT.connect("ESP32_Receiver");
      }
    }
  }

  // --- STEP 3: Process Incoming Data (Regardless of Source) ---
  if (dataReceived && incomingData.length() > 0) {
    Serial.print("Data from [");
    Serial.print(activeSource);
    Serial.print("]: ");
    Serial.println(incomingData);
    // You can parse your lat/lng/heading variables here just like before!
  }

  // --- STEP 4: Update OLED Display Status ---
  display.clearDisplay();
  display.setCursor(0, 0);
  display.print(F("Hdg:")); display.print(baseHeading);
  display.print(F(" Src:")); display.println(currentMode == MODE_SERIAL_UNO ? F("UNO") : F("BT"));

  display.setCursor(0, 20);
  display.print(F("Status: ")); 
  display.println(dataReceived ? F("Active Link") : F("Searching..."));

  display.setCursor(0, 40);
  display.print(F("Mode: ")); display.println(activeSource);
  
  display.display();
  delay(100);
}