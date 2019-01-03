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
          saveToFS();
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
