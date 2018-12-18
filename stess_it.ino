#include <Adafruit_ADS1015.h>
#include <PulseSensorPlayground.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <stdlib.h>
#include <InfluxDb.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "RTClib.h"

//******************* Wi-Fi credentials ******************
const char* ssid = "ZiggoC6C6A6C";
const char* password = "3YjjjrzteyFk";
//const char* ssid = "AndroidAP_63";
//const char* password = "00000000";
//********************************************************

#define INFLUXDB_HOST "192.168.178.157"
#define INFLUXDB_PORT "8086"
#define INFLUXDB_DATABASE "measurements"

// Create a TCP Server on port 8085
WiFiServer server(5045);
WiFiClient client;

Influxdb influx(INFLUXDB_HOST, 8086);   //Create connection with the DB.
String const USER_NAME = "Dimitriy";
boolean dbAvailable = true;

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
int measurementPin = D0;

byte const windowSize = 4;
int myData[windowSize];

unsigned long previousMillis = 0;
unsigned long previousMillisDb = 0;
char buf[12];

RTC_DS3231 rtc;

//********************************LCD config**************************************
LiquidCrystal_I2C lcd(0x27, 16, 2);
int setCursorPoint;
uint8_t arrowUp[8] = {0x04, 0x0E, 0x15, 0x04, 0x04, 0x04, 0x04, 0x00};
uint8_t arrowDown[8] = {0x04, 0x04, 0x04, 0x04, 0x15, 0x0E, 0x04, 0x00};
//********************************************************************************

void setup() {
  ads.setGain(GAIN_ONE);                  //DO NOT EXCEED +/- 4.096V on sensor reading, otherwise you may damage the ADC.
  ads.begin();

  lcd.begin();
  lcd.backlight();
  lcd.createChar(0, arrowUp);
  lcd.createChar(1, arrowDown);
  lcd.home();
  lcd.print("Works, please");

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

  SD.begin(D8);

  influx.setDb(INFLUXDB_DATABASE);
  server.begin();   //Start the TCP server
  haveBaseline = readBaselineSD();
  setMinHRV();

  rtc.begin();
  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
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
    if (digitalRead(measurementPin) && isReady) {
      getGSR();
      initBPM = SD.open("initBPM.txt", FILE_WRITE);
      initHRV = SD.open("initHRV.txt", FILE_WRITE);
      initGSR = SD.open("initGSR.txt", FILE_WRITE);
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

void sendToTcpClient() {
  client = server.available();
  if (client) {
    while (client.connected()) {
      getBPM();
      if (millis() - previousMillis > 3000 && QS == true) {
        getGSR();
        client.write(itoa(lastBPM, buf, 10));
        client.write(',');
        client.write(itoa(IBI, buf, 10));
        client.write(',');
        client.write(itoa(lastGSR, buf, 10));
        client.write(',');
        client.write(itoa(stressLevel, buf, 10));
        client.write("\n");                           //new line marks the end of this reading
        previousMillis = millis();
        QS = false;                         // Quantified Self "QS" true when arduino finds a heartbeat
      }
      if (dbAvailable && isReady) {
        getGSR();
        //  dbAvailable = saveToDB();
        saveToDB();
        isReady = false;
      }
      delay(20);
    }
  }
}

boolean saveToDB() {
  InfluxData row(USER_NAME);
  row.addTag("device", "alpha");
  row.addValue("bpm", lastBPM);
  row.addValue("ibi", IBI);
  row.addValue("hrv", HRV);
  row.addValue("gsr", lastGSR);
  row.addValue("sss", stressLevel);
  previousMillisDb = millis();
  return influx.write(row);
}

void getBPM() {
  int16_t adc0;
  unsigned long curTime = millis();

  adc0 = ads.readADC_SingleEnded(0);                      //TODO: Select the correct ADC channel. I have selected A0 here

  sampleCounter += curTime - lastTime;                   // keep track of the time in mS with this variable
  lastTime = curTime;
  int N = sampleCounter - lastBeatTime;                 // monitor the time since the last beat to avoid noise

  //  find the peak and trough of the pulse wave
  if (adc0 < thresh and N > (IBI / 5.0) * 3.0) {          // avoid dichrotic noise by waiting 3/5 of last IBI
    if (adc0 < T) {                                      // T is the trough
      T = adc0;                                         //keep track of lowest point in pulse wave
    }
  }

  if (adc0 > thresh and  adc0 > P) {           // thresh condition helps avoid noise
    P = adc0;                                 //P is the peak, keep track of highest point in pulse wave
  }

  //  NOW IT'S TIME TO LOOK FOR THE HEART BEAT
  // signal surges up in value every time there is a pulse
  if (N > 250) {                                   // avoid high frequency noise
    if  (adc0 > thresh and havePulse == false and N > (IBI / 5.0) * 3.0) {
      havePulse = true;                               //set the havePulse flag when we think there is a pulse
      IBI = sampleCounter - lastBeatTime;            // measure time between beats in mS
      lastBeatTime = sampleCounter;                 //keep track of time for next pulse
      if (secondBeat) {                        // if this is the second beat, if secondBeat == TRUE
        secondBeat = false;
        for (int i = 0; i < 10; i++) {             // seed the running total to get a realisitic BPM at startup
          rate[i] = IBI;
        }
      }
      if (firstBeat) {
        firstBeat = false;                   // clear firstBeat flag
        secondBeat = true;                   // set the second beat flag
        return;                             // IBI value is unreliable so discard it
      }

      int runningTotal = 0;                  // clear the runningTotal variable
      for (int i = 0; i < 9; i++)  {
        rate[i] = rate[i + 1];                  // and drop the oldest IBI value
        runningTotal += rate[i];              // add up the 9 oldest IBI values
      }
      rate[9] = IBI;                          // add the latest IBI to the rate array
      runningTotal += rate[9];                // add the latest IBI to runningTotal
      runningTotal /= 10;                     // average the last 10 IBI values
      int bpm = 60000 / runningTotal;            // how many beats can fit into a minute? that's BPM!
      lastBPM = getSMA(bpm);
      int tempHRV = (int)pow(prevIBI - IBI, 2);
      sumHRV += tempHRV;
      counterHRV++;
      if (counterHRV == 14) {
        HRV = (int) sqrt(sumHRV / counterHRV);    //Calculating RMSSD
        counterHRV = 0;
        sumHRV = 0;
        if (haveBaseline) {
          stressLevel = checkStress();
          saveToSD();
          setTextLCD();
        }
        isReady = true;
      }
      QS = true;
      prevIBI = IBI;
    }
  }

  if (adc0 < thresh and havePulse == true) {  // when the values are going down, the beat is over
    havePulse = false;                         // reset the havePulse flag so we can do it again
    amp = P - T;                           // get amplitude of the pulse wave
    thresh = amp / 2 + T;                    // set thresh at 50% of the amplitude
    P = thresh;                            // reset these for next time
    T = thresh;
  }

  if (N > 2500) {                          // if 2.5 seconds go by without a beat
    thresh = 13900;                          // set thresh default
    P = 13600;                               // set P default
    T = 13600;                               // set T default
    IBI = 600;
    prevIBI = 600;
    lastBeatTime = sampleCounter;          // bring the lastBeatTime up to date
    firstBeat = true;                      // set these to avoid noise
    secondBeat = false;                    // when we get the heartbeat back
    QS = false;
    HRV = -1;
    return;
  }
  delay(0);
}

void getGSR() {
  int sum = 0;
  for (int i = 0; i < 12; i ++) {
    sum += ads.readADC_SingleEnded(2);
  }
  lastGSR = sum / 12;
}

int getSMA(int newValue) {
  for (int i = 0; i < windowSize - 1 ; i++) {
    myData [ i] = myData[i + 1];
  }
  myData[windowSize - 1] = newValue;
  long total;
  for (int i = 0; i < windowSize; i++) {
    total += myData [ i];
  }
  return total / windowSize;
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

boolean readBaselineSD() {
  int sumBPM = 0;
  int sumHRV = 0;
  int sumGSR = 0;
  int n = 3;         //number of measurements to use for calculating baseline
  if (SD.exists("baseline.txt")) {
    baselineFile = SD.open("baseline.txt", FILE_READ);
    baselineBPM = baselineFile.readStringUntil('\n').toInt();
    baselineHRV = baselineFile.readStringUntil('\n').toInt();
    baselineGSR = baselineFile.readStringUntil('\n').toInt();
    baselineFile.close();
    lcd.clear();
    lcd.print(String(baselineBPM));
    lcd.print(",");
    lcd.print(String(baselineHRV));
    lcd.print(",");
    lcd.print(String(baselineGSR));
    return true;
  } else {
    initBPM = SD.open("initBPM.txt", FILE_READ);
    while (initBPM.available()) {
      String lineB = initBPM.readStringUntil('\n');
      sumBPM += lineB.toInt();
      numberOfLines++;
      if (numberOfLines == n) {
        initHRV = SD.open("initHRV.txt", FILE_READ);
        initGSR = SD.open("initGSR.txt", FILE_READ);
        numberOfLines = 0;
        while (initHRV.available()) {
          String lineH = initHRV.readStringUntil('\n');
          sumHRV += lineH.toInt();
          numberOfLines++;
          if (numberOfLines == n) {
            break;
          }
        }
        numberOfLines = 0;
        while (initGSR.available()) {
          String lineG = initGSR.readStringUntil('\n');
          sumGSR += lineG.toInt();
          numberOfLines++;
          if (numberOfLines == n) {
            break;
          }
        }
        initBPM.close();
        initHRV.close();
        initGSR.close();
        baselineBPM = sumBPM / n;
        baselineHRV = sumHRV / n;
        baselineGSR = sumGSR / n;
        baselineFile = SD.open("baseline.txt", FILE_WRITE);
        baselineFile.println(baselineBPM);
        baselineFile.println(baselineHRV);
        baselineFile.println(baselineGSR);
        baselineFile.flush();
        baselineFile.close();
        return true;
      }
    }
    return false;
  }
}

void setMinHRV() {
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

void saveToSD() {
  dataFile = SD.open("dataFile.txt", FILE_WRITE);
  dataFile.print(getTime());
  dataFile.print(',');
  dataFile.print(lastBPM);
  dataFile.print(',');
  dataFile.print(HRV);
  dataFile.print(',');
  dataFile.print(lastGSR);
  dataFile.print(',');
  dataFile.println(stressLevel);
  dataFile.flush();
  dataFile.close();
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

void setTextLCD() {
  lcd.clear();

  String text1 = "BPM";
  text1 += String(lastBPM);
  lcd.setCursor(2, 0);
  if (lastBPM > (baselineBPM + baselineBPM * 25 / 100)) {
    lcd.print("BPM");
    lcd.write(0);
  } else if (lastBPM < (baselineBPM + baselineGSR * 25 / 100)) {
    lcd.print("BPM");
    lcd.write(1);
  } else {
    lcd.print("OK");
  }
  if (HRV > baselineHRV) {
    lcd.print("HRV");
    lcd.write(0);
  } else {
    lcd.print("HRV");
    lcd.write(1);
  }
  if (lastGSR > (baselineGSR + baselineGSR * 25 / 100)) {
    lcd.print("GSR");
    lcd.write(0);
  } else if (lastGSR < (baselineGSR + baselineGSR * 25 / 100)) {
    lcd.print("GSR");
    lcd.write(1);
  } else {
    lcd.print("OK");
  }
  String text2;
  if (stressLevel == 0) {
    text2 = "Normal";
  } else {
    text2 = "Stress Level";
    text2 += " ";
    text2 += String(stressLevel);
  }
  //    setCursorPoint = (16 - text.length()) / 2;
  lcd.setCursor(0, 1);
  lcd.print(text2);
}

