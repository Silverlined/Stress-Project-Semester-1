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
