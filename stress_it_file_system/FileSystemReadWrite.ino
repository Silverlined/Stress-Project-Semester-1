boolean readBaselineFS() {
  int sumBPM = 0;
  int sumHRV = 0;
  int sumGSR = 0;
  int n = 3;         //number of measurements to use for calculating baseline
  SPIFFS.begin();
  if (SPIFFS.exists("/BASELINE.txt")) {
    baselineFile = SPIFFS.open("/BASELINE.txt", "r");
    baselineBPM = baselineFile.readStringUntil('\n').toInt();
    baselineHRV = baselineFile.readStringUntil('\n').toInt();
    baselineGSR = baselineFile.readStringUntil('\n').toInt();
    baselineFile.close();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Baseline params:");
    lcd.setCursor(0, 1);
    lcd.print(String(baselineBPM));
    lcd.print(", ");
    lcd.print(String(baselineHRV));
    lcd.print(", ");
    lcd.print(String(baselineGSR));
    return true;
  } else {
    initBPM = SPIFFS.open("/INITBPM.txt", "r");
    while (initBPM.available()) {
      String lineB = initBPM.readStringUntil('\n');
      sumBPM += lineB.toInt();
      numberOfLines++;
      if (numberOfLines == n) {
        initHRV = SPIFFS.open("/INITHRV.txt", "r");
        initGSR = SPIFFS.open("/INITGSR.txt", "r");
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
        baselineFile = SPIFFS.open("/BASELINE.txt", "w");
        baselineFile.println(baselineBPM);
        Serial.println(baselineBPM);
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

void saveToFS() {
  dataFile = SPIFFS.open("/DATAFILE.txt", "a");
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
