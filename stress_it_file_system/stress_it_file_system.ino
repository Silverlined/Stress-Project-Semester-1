#include "FS.h"
#include <Adafruit_ADS1015.h>
#include <ArduinoJson.h>
#include <PulseSensorPlayground.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <stdlib.h>
#include <InfluxDb.h>
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "RTClib.h"

//******************* Wi-Fi credentials ******************
//const char* ssid = "ZiggoC6C6A6C";
//const char* password = "3YjjjrzteyFk";
char ssid[50];
char password[50];
//********************************************************

#define INFLUXDB_HOST "192.168.43.227" // local ip address, right now it is not valid.
#define INFLUXDB_PORT "8086"
#define INFLUXDB_DATABASE "measurements"

// Create a TCP Server on port "X"
WiFiServer server(5045);
WiFiClient client;

//********************************InfluxDB config*********************************
Influxdb influx(INFLUXDB_HOST, 8086);   //Create connection with the DB.
String const USER_NAME = "Dimitriy";
boolean dbAvailable = true;
//********************************************************************************

Adafruit_ADS1115 ads;

File baselineFile;
File initBPM;
File initHRV;
File initGSR;
File dataFile;
byte numberOfLines = 0;

//********************************Pulse Sensor Variables*************************
int thresh = 13900;
int P = 13600;                 // peak at around 1/2 the input range of 0...26,400
int T = 13600;
unsigned long sampleCounter = 0;
unsigned long lastBeatTime = 0;
unsigned long lastTime;
boolean firstBeat = true;
boolean secondBeat = false;
boolean havePulse = false;
boolean QS = false;
int prevIBI = 600;
int IBI = 600;                  // 600ms per beat = 100 Beats Per Minute (BPM) (Interbeat interval)
int rate[10];
int amp = 2600;
uint8_t lastBPM = 1;
int HRV = -1;
int counterHRV = 0;
int sumHRV = 0;
//********************************************************************************
int lastGSR = -1;

int stressLevel = 0;
boolean isReady = false;

int baselineBPM = -1;
int baselineHRV = -1;
int baselineGSR = -1;
boolean haveBaseline;

int const ageOfPatient = 18;

int rangeHRV[] = {40, 35, 26};
int minHRV;
int measurementPin = D3;

byte const windowSize = 4;
int myData[windowSize];

unsigned long previousMillis = 0;
unsigned long previousMillisDb = 0;
char buf[12];

RTC_DS3231 rtc;
//********************************LCD special chars**************************************
LiquidCrystal_I2C lcd(0x27, 16, 2);
int setCursorPoint;
uint8_t arrowUp[8] = {0x04, 0x0E, 0x15, 0x04, 0x04, 0x04, 0x04, 0x00};
uint8_t arrowDown[8] = {0x04, 0x04, 0x04, 0x04, 0x15, 0x0E, 0x04, 0x00};
//********************************************************************************

void setup() {
  SPIFFS.begin();
  setWifiConfig();
  ads.setGain(GAIN_ONE);                  //DO NOT EXCEED +/- 4.096V on sensor reading, otherwise you may damage the ADC.
  ads.begin();

  lcd.begin();
  lcd.backlight();
  lcd.createChar(0, arrowUp);
  lcd.createChar(1, arrowDown);
  lcd.home();
  lcd.print("Setting up...");

  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("");
  //Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  influx.setDb(INFLUXDB_DATABASE);
  server.begin();   //Start the TCP server
  haveBaseline = readBaselineFS();
  setLowerThresholdRMSSD();

  rtc.begin();
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  pinMode(D3, INPUT);
}

void setWifiConfig() {
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");
          strcpy(ssid, json["ssid"]);
          strcpy(password, json["password"]);
        } else {
          Serial.println("failed to load json config");
        }
        configFile.close();
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
}

void getBPM();
void getGSR();

void loop() {
  if (haveBaseline) {
    sendToTcpClient();
    getBPM();
    if (dbAvailable && isReady) {
      getGSR();
      //  dbAvailable = saveToDB();
      saveToDB();
      isReady = false;
    }
  } else {
    getBPM();
    if (!digitalRead(measurementPin) && isReady) {
      Serial.println("yes");
      getGSR();
      initBPM = SPIFFS.open("/INITBPM.txt", "a");
      initHRV = SPIFFS.open("/INITHRV.txt", "a");
      initGSR = SPIFFS.open("/INITGSR.txt", "a");
      initBPM.println(lastBPM);
      initHRV.println(HRV);
      initGSR.println(lastGSR);
      initBPM.flush();
      initHRV.flush();
      initGSR.flush();
      initBPM.close();
      initHRV.close();
      initGSR.close();
      isReady = false;
    }
  }
  delay(20);
}

void getGSR() {
  int sum = 0;
  for (int i = 0; i < 12; i ++) {
    sum += ads.readADC_SingleEnded(2);
  }
  lastGSR = sum / 12;
}

int checkStress() {
  if (haveBaseline) {
    getGSR();
    int level = 0;
    if (HRV < baselineHRV && HRV < minHRV) {
      level = 2;
      if (lastGSR > (baselineGSR + baselineGSR * 30 / 100) || HRV < 20) {
        level = 3;
      }
      return level;
    } else if (lastBPM > (baselineBPM + baselineBPM * 40 / 100)) {
      level = 2;
      return level;
    } else if (lastBPM > (baselineBPM + baselineBPM * 25 / 100) && lastGSR > (baselineGSR + baselineGSR * 30 / 100) ) {
      level = 1;
      return level;
    }
  }
  return 0;
}

void setLowerThresholdRMSSD() {
  if (ageOfPatient < 32) {
    minHRV = rangeHRV[0];
    return;
  } else if (ageOfPatient < 50) {
    minHRV = rangeHRV[1];
    return;
  } else {
    minHRV = rangeHRV[2];
    return;
  }
}

String getTime() {
  DateTime now = rtc.now();
  String curTime;
  curTime += String(now.year(), DEC);
  curTime += '/';
  curTime += String(now.month(), DEC);
  curTime += '/';
  curTime += String(now.day(), DEC);
  curTime += ' ';
  curTime += String(now.hour(), DEC);
  curTime += ':';
  curTime += String(now.minute(), DEC);
  curTime += ':';
  curTime += String(now.second(), DEC);
  return curTime;
}
