#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>

#include <locale>
#include <codecvt>
#include <string>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#define isLinkingStatePreferencesName "isLinkingState"

#define WIFI_SSID "Hasenet"
#define WIFI_PASSWORD "hase%haesin"

#define API_KEY "AIzaSyAW71ttFo_XxjWSslGRmp5M9qt2vbhCE3s"
#define FIRESTORE Firebase.Firestore

#define DATABASE_EMAIL "erik.e@kls.schule.koeln"
#define DATABASE_PASSWORD "test123"
#define FIREBASE_PROJECT_ID "intelligent-plant"

#define WATER_SENSOR_PIN 36
#define LIGHT_SENSOR_PIN 33
#define TEMPERATURE_SENSOR_PIN 39
#define PUMP_RELAY_PIN 32

#define WET 1100
#define DRY 3700

#define DARK 4095
#define BRIGHT 300

#define HIGH_TEMP 40 // analog Value: 1122
#define LOW_TEMP 0 //analogValue: 624

#define ADC_VREF_mV    1200.0 // in millivolt
#define ADC_RESOLUTION 4096.0

#define MIN_WATER_PERCENTAGE 50

Preferences preferences;
bool isLinkingState;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupSuccess = false;
unsigned maxConnectingAttempts = 50;

int waterValue = 0;
int waterPercantageValue = 0;

int lightValue = 0;
int lightPercantageValue = 0;
int lightLuxValue = 0;

int tempValue = 0;
int tempPercantageValue = 0;
int tempC = 0;
int tempF = 0;

float milliVolt = 0;

String uid;
String plant = "plant_0";
String pathToPlant;

LiquidCrystal_I2C lcd_disp(0x27, 16, 2);

class Display {
    public:
        void initDisplay() {
            lcd_disp.init();
            lcd_disp.backlight();
            lcd_disp.setCursor(0, 0);
            lcd_disp.print("Initialization");
            delay(2000);
        }

        void printNew1Row(String message, int letterNum, int row) {
            lcd_disp.clear();
            lcd_disp.setCursor(letterNum, row);
            lcd_disp.print(message);
        }
        
        void print1Row(String message, int letterNum, int row) {
            lcd_disp.setCursor(letterNum, row);
            lcd_disp.print(message);
        }

        void clear() {
          lcd_disp.clear();
        }
};

Display lcd;

void setup()
{
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  preferences.begin("intelplant", false);

  lcd.initDisplay();

  isLinkingState = preferences.getBool(isLinkingStatePreferencesName, true);

  Serial.print("Connecting to ");
  Serial.print(WIFI_SSID);
  Serial.print("\n");

  lcd.printNew1Row("Connecting to", 0, 0);
  lcd.print1Row(WIFI_SSID, 0, 1);

  int connectingAttempts = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    if (connectingAttempts < maxConnectingAttempts)
    {
      Serial.print(".");
    }
    else
    {
      Serial.print("Error while Connecting! - Timeout -");
    }

    connectingAttempts++;
    delay(300);
  }

  Serial.println();
  Serial.print("Connected to WiFi with IP: ");
  Serial.print(WiFi.localIP());
  Serial.println();
  
  lcd.printNew1Row("Connected. IP:", 0, 0);
  lcd.print1Row(WiFi.localIP().toString(), 0, 1);

  delay(1000);

  preferences.putBool(isLinkingStatePreferencesName, true); // NO!

  if (isLinkingState) {
    lcd.printNew1Row("Please Link with", 0, 0);
    lcd.print1Row("App to get Started", 0, 1);
    delay(5000);
    lcd.printNew1Row("Linked!", 0, 0);
    preferences.putBool(isLinkingStatePreferencesName, false);
    // Link
  }

  delay(1000);

  config.api_key = API_KEY;
  
  auth.user.email = DATABASE_EMAIL;
  auth.user.password = DATABASE_PASSWORD;

  config.token_status_callback = tokenStatusCallback;
  Firebase.reconnectWiFi(true);

  Firebase.begin(&config, &auth);

  Serial.println("Logging In");

  lcd.printNew1Row("Logging In", 0, 0);
  lcd.print1Row(DATABASE_EMAIL, 0, 1);

  while ((auth.token.uid) == "") {
    Serial.print('.');
    delay(1000);
  }

  signupSuccess = true;

  uid = auth.token.uid.c_str();
  Serial.print("User UID: ");
  Serial.print(uid);
  Serial.println();
  pathToPlant = "Users/" + uid + "/Plants/" + plant;
  Serial.print("Path to Plant: ");
  Serial.print(pathToPlant);
  Serial.println();
  
  lcd.printNew1Row("Login Success. UID:", 0, 0);
  lcd.print1Row(uid, 0, 1);

  delay(4000);

  lcd.printNew1Row("Starting Program", 0, 0);

  delay(4000);

  lcd.clear();

  // Pinmodes
  pinMode(WATER_SENSOR_PIN, INPUT);
  pinMode(LIGHT_SENSOR_PIN, INPUT);
  pinMode(TEMPERATURE_SENSOR_PIN, INPUT);
  pinMode(PUMP_RELAY_PIN, OUTPUT);
}

void loop() {

  // Get and Process water Value
  waterValue = analogRead(WATER_SENSOR_PIN);
  waterPercantageValue = map(waterValue, WET, DRY, 100, 0);

  lightValue = analogRead(LIGHT_SENSOR_PIN);
  lightPercantageValue = map(lightValue, BRIGHT, DARK, 100, 0);
  lightLuxValue = (2500 / (lightValue * 4095)) - 500;

  tempValue = analogRead(TEMPERATURE_SENSOR_PIN);
  milliVolt = tempValue * (ADC_VREF_mV / ADC_RESOLUTION);
  tempC = milliVolt / 10;
  tempF = (tempC * 1.8) + 32;
  tempPercantageValue = map(tempC, HIGH_TEMP, LOW_TEMP, 100, 0);

  if (waterPercantageValue <= MIN_WATER_PERCENTAGE)
  {
    digitalWrite(PUMP_RELAY_PIN, HIGH);
  } else
  {
    digitalWrite(PUMP_RELAY_PIN, LOW);
  }

  FirebaseJson PlantData;

  if (Firebase.ready() && signupSuccess == true && (millis() - sendDataPrevMillis > 5000 || sendDataPrevMillis == 0))
  {
    sendDataPrevMillis = millis();

    Serial.println(PlantData.raw());
    Serial.println();

    if (Firebase.Firestore.getDocument(&fbdo, FIREBASE_PROJECT_ID, "", pathToPlant)) {
      Serial.print("read data: ");
      Serial.print(fbdo.payload().c_str());
      Serial.println();
      PlantData.setJsonData(fbdo.payload().c_str());
        
      PlantData.set("fields/data/mapValue/fields/Water/mapValue/fields/unneutralized/integerValue", std::to_string(waterValue));
      PlantData.set("fields/data/mapValue/fields/Water/mapValue/fields/inPercent/integerValue", std::to_string(waterPercantageValue));

      PlantData.set("fields/data/mapValue/fields/Light/mapValue/fields/inPercent/integerValue", std::to_string(lightPercantageValue));
      PlantData.set("fields/data/mapValue/fields/Light/mapValue/fields/inLux/integerValue", std::to_string(100));
      PlantData.set("fields/data/mapValue/fields/Light/mapValue/fields/unneutralized/integerValue", std::to_string(lightValue));

      PlantData.set("fields/data/mapValue/fields/Temperature/mapValue/fields/inPercent/integerValue", std::to_string(tempPercantageValue));
      PlantData.set("fields/data/mapValue/fields/Temperature/mapValue/fields/in°C/integerValue", std::to_string(tempC));
      PlantData.set("fields/data/mapValue/fields/Temperature/mapValue/fields/in°F/integerValue", std::to_string(tempF));
      PlantData.set("fields/data/mapValue/fields/Temperature/mapValue/fields/unneutralized/integerValue", std::to_string(tempValue));
      
      Serial.print("set data: ");
      Serial.print(PlantData.raw());
      Serial.println();
    } else {
      Serial.println(fbdo.errorReason());
      return;
    }

    std::vector<fb_esp_firestore_document_write_t> writes;
    fb_esp_firestore_document_write_t write;
    write.update_document_path = pathToPlant;
    write.update_document_content = PlantData.raw();
    write.type = fb_esp_firestore_document_write_type_update;
    writes.push_back(write);

    if (Firebase.Firestore.commitDocument(&fbdo, FIREBASE_PROJECT_ID, "", writes))
      Serial.printf("ok\n%s\n\n", fbdo.payload().c_str());
    else
      Serial.println(fbdo.errorReason());
    
    lcd.printNew1Row("Sent new Data:", 0, 0);
    lcd.print1Row("W: ", 0, 1);
    lcd.print1Row(std::to_string(waterPercantageValue).c_str(), 2, 1);
    lcd.print1Row("L:", 5, 1);
    lcd.print1Row(std::to_string(lightPercantageValue).c_str(), 7, 1);
    lcd.print1Row("T: ", 10, 1);
    lcd.print1Row(std::to_string(tempC).c_str(), 12, 1);

    /*
    // Sending data to RTDB
    if (Firebase.setInt(fbdo, "Sensor/water_value", waterValue))
    {
      Serial.println();
      Serial.print(waterValue);
      Serial.print(" - Successfully saved to ");
      Serial.print(fbdo.dataPath());
      Serial.print(" (");
      Serial.print(fbdo.dataType());
      Serial.print(")");
    }
    else
    {
      Serial.println("FAILED: " + fbdo.errorReason());
    }

    if (Firebase.setInt(fbdo, "Sensor/water_percentage_value", waterPercantageValue))
    {
      Serial.println();
      Serial.print(waterPercantageValue);
      Serial.print(" - Successfully saved to ");
      Serial.print(fbdo.dataPath());
      Serial.print(" (");
      Serial.print(fbdo.dataType());
      Serial.print(")");
    }
    else
    {
      Serial.println("FAILED: " + fbdo.errorReason());
    }
    delay(300);*/
  }
}