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
