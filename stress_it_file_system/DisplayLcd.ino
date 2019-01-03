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
  String stress_txt;
  if (stressLevel == 0) {
    stress_txt = "Normal";
  } else {
    stress_txt = "Stress Level";
    stress_txt += " ";
    stress_txt += String(stressLevel);
  }
  lcd.setCursor(0, 1);
  lcd.print(stress_txt);
}
