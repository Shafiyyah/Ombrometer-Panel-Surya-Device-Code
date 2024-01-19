
//Post Battery status, Signal Status, Kuota, Masa Tenggang
int vPostDataPeriodic() {
  char resource[128] = "/Ombrometer/periodicOTALoRa/";
  sprintf(resource,"/Ombrometer/periodicOTALoRa/%lu",chipID);
  Serial.println(resource);

  unsigned long time_out=millis();
  digitalWrite(SIM800_Trigger,HIGH);
  modem.init();

  String modemInfo = modem.getModemInfo();
  Serial.print(F("Modem: "));
  Serial.println(modemInfo);
  
  Serial.print("Waiting for network...");
  if (!modem.waitForNetwork() && (millis()-time_out>15000)) {
    Serial.println(" Fail");
    return 0;
  }
  Serial.println(" Success");
  if (modem.isNetworkConnected()) { Serial.println("Network connected"); }

  signalSIM800 = -1;
  while (signalSIM800 > 31 || signalSIM800 < 0) {
    signalSIM800 = modem.getSignalQuality();
  }
  Serial.print("Signal Strenght: ");
  Serial.println(signalSIM800);


  
  // GPRS connection parameters are usually set after network registration
  Serial.print(F("Connecting to "));
  Serial.print(apn);
  if (!modem.gprsConnect(apn, user, pass)) {
    Serial.println(" fail");
    return 0;
  }
  Serial.println(" success");
  if (modem.isGprsConnected()) { Serial.println("GPRS connected"); }

  getGSMTIME();
  now = myRTC.now();
  unsigned long unix=now.unixtime()-25200;

  //Check changed day?
  if ((int)now.day()!=oldDay) {
    bSendSD=1;  
  }

  if(rainrate>=alert) {
    bAlert=1;
  }

  const char contentType[64] = "application/x-www-form-urlencoded";
  char postData[256];
  
  if (bAlert) {
    sprintf(postData,"Timestamp=%ld&RainStart=%d&RainStop=%d&Battery=%f&Signal=%d&Alert=1&RainRateDay=%f&Firmware=%d\0",unix,bRainStart,bRainStop,vBatt,signalSIM800,rainrateDay,oldVersion);
    bAlerted=1;
  } else {
    sprintf(postData,"Timestamp=%ld&RainStart=%d&RainStop=%d&Battery=%f&Signal=%d&Alert=0&RainRateDay=%f&Firmware=%d\0",unix,bRainStart,bRainStop,vBatt,signalSIM800,rainrateDay,oldVersion);
  }
  
  char bufferText[64];
  if (bRainStop) {
    sprintf(bufferText,"&RainRate=%f\0",oldRainrate);  
  } else {
    sprintf(bufferText,"&RainRate=%f\0",rainrate);  
  }

  strcat(postData,bufferText);
   
  http.connectionKeepAlive();  // Currently, this is needed for HTTPS
  http.post(resource, contentType, postData);

  // read the status code and body of the response
  int statusCode = http.responseStatusCode();
  if (statusCode!=200) {
    digitalWrite(SIM800_Trigger,LOW);
    return statusCode;
  }
  String response = http.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);
  sscanf(response.c_str(),"%d,%f,%f,%d,%d,%d,%d,%s ,%s ,%s ,%s ,%s ,%s ,%s ",&period,&tipmm,&alert,&stopTime,&periodSD,&resetDevice,&newVersion,&LoRaIO[0],&LoRaIO[1],&LoRaIO[2],&LoRaIO[3],&LoRaIO[4],&LoRaIO[5],&LoRaIO[6]);
//  LoRaMode=TRANSMIT_MODE;
  bDoneLoRa=0;

  if (period!=oldPeriod) {
    EEPROM.put(0,period);
    EEPROM.commit();
  }
  if (tipmm!=oldTipmm) {
    EEPROM.put(sizeof(period), tipmm);
    EEPROM.commit();
  }
  if (alert!=oldAlert) {
    EEPROM.put(sizeof(period)+sizeof(tipmm), alert);
    EEPROM.commit();
  }

  if (stopTime!=oldStopTime) {
    EEPROM.put(sizeof(period)+sizeof(tipmm)+sizeof(alert), stopTime);
    EEPROM.commit();
  }
  if (periodSD!=oldPeriodSD) {
    EEPROM.put(sizeof(period)+sizeof(tipmm)+sizeof(alert)+sizeof(stopTime), periodSD);
    EEPROM.commit();
  }

  if (resetDevice==1) {
    ESP.restart();
  }

  Serial.printf("OldVersion: %d, newVersion: %d\n",oldVersion, newVersion); 
  if (newVersion!=oldVersion) {
    bOTAStart=1;
    OTAretry=0;
    Serial.println("UPDATEOTA");
  }
  
  oldPeriod=period;
  oldTipmm=tipmm;
  oldAlert=alert;
  oldStopTime=stopTime;
  oldPeriodSD=periodSD;
  
  digitalWrite(SIM800_Trigger,LOW);
  return statusCode;
}


//Post SD Card Data Every Day (?)
int vPostDataSDCard() {
  char resource[128] = "/Ombrometer/upload/";
  sprintf(resource,"/Ombrometer/upload/%lu",chipID);
  Serial.println(resource);

  unsigned long time_out=millis();
  digitalWrite(SIM800_Trigger,HIGH);
  modem.init();

  String modemInfo = modem.getModemInfo();
  Serial.print(F("Modem: "));
  Serial.println(modemInfo);
  
  Serial.print("Waiting for network...");
  if (!modem.waitForNetwork() && (millis()-time_out>15000)) {
    Serial.println(" Fail");
    return 0;
  }
  Serial.println(" Success");
  if (modem.isNetworkConnected()) { Serial.println("Network connected"); }

  // GPRS connection parameters are usually set after network registration
  Serial.print(F("Connecting to "));
  Serial.print(apn);
  if (!modem.gprsConnect(apn, user, pass)) {
    Serial.println(" fail");
    return 0;
  }
  Serial.println(" success");
  if (modem.isGprsConnected()) { Serial.println("GPRS connected"); }

  

  char bufferTitle[64]="";
  now = myRTC.now();
  sprintf(bufferTitle, "/Ombrometer_%d-%d-%d.csv",now.year(),now.month(),oldDay);
  Serial.println(bufferTitle);

  vSetupSDCard();
  CRC32 crc;
  String NilaiCRC32;
  File dataFile = SD.open(bufferTitle);
  if (dataFile) {
    Serial.println("available");
    while (dataFile.available()) {
      uint8_t byteRead = dataFile.read();
      crc.update(byteRead);
    }
    uint32_t CRCResult = crc.finalize();
    char CRCResultChar[9]; // Menggunakan 9 karakter untuk nilai CRC32
    sprintf(CRCResultChar, "%08x", CRCResult); // Mengatur lebar tetap 8 karakter dengan nol di depan jika perlu
    Serial.print("Calc. CRC32:    toCharArray 0x");
    Serial.println(CRCResultChar);
    NilaiCRC32 = String(CRCResultChar);
    dataFile.close();
  }
  else{
    Serial.println("Error Opening file");
  }
  
  char head[256] = "";
  sprintf(head,"--SNoveLab\r\nContent-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\nContent-Type: text/plain\r\n\r\n",bufferTitle);
  int i=0;
  while(head[i]!='\0') {
    i++;
  }
  Serial.printf("length: %d\n",i);
  String tail = "\r\n--SNoveLab--\r\n";

  String headData ="\r\n--SNoveLab\r\nContent-Disposition: form-data; name=\"CRC32\"\r\n\r\n";
  String postData = String(NilaiCRC32);

  File file = SD.open(bufferTitle);
  if(!file){
    Serial.println("Failed to open file for reading");
    return 0;
  }

  uint32_t textLen = file.size();
  uint32_t extraLen = i + tail.length();
  uint32_t totalLen = textLen + extraLen + postData.length()+headData.length();

  http.connectionKeepAlive();
  http.beginRequest();
  int codePost = http.post(resource);

  Serial.print(F("Returned status after post = "));
  Serial.println(codePost); 
  http.sendHeader("Content-Length", totalLen);
  http.sendHeader("Content-Type: multipart/form-data; boundary=SNoveLab");
  http.beginBody();
  http.println();
  http.print(head);

  
  
  unsigned char tempBuffer[512];
  uint8_t *tBuf = (unsigned char*)tempBuffer;
  int sent_size;
  bool bFailSendData = 0;
  int x = 0;
  for (long i = 0; i < textLen && !bFailSendData; i += CHUNCK_SIZE) {
    size_t bytesToRead = std::min(static_cast<size_t>(CHUNCK_SIZE), static_cast<size_t>(textLen - i));
    size_t bytesRead = file.read(tempBuffer, bytesToRead);
    Serial.print("Bytes Read: ");
    Serial.println(bytesRead);
    // Print the content of tempBuffer for debugging
    Serial.print("tempBuffer: ");
    for (size_t j = 0; j < bytesRead; j++) {
      Serial.print(tempBuffer[j]);
      Serial.print(" ");
    }
    Serial.println();
    sent_size = http.write(tempBuffer, bytesRead);
    Serial.print("Sent Size: ");
    Serial.println(sent_size);

    if (sent_size != bytesRead) {
      bFailSendData = true;
      Serial.println("Failed to send all data");
    }
    // Delay for stability
    // delay(10);
  }
  file.close();
  
  //Done reading data turn off sd card
  vTurnOffSD();
  
  http.flush();
  http.print(headData); 
  http.print(postData);
  http.print(tail);
  http.endRequest();

  int status = http.responseStatusCode();
  Serial.printf("Status: %d\n",status);
  
  digitalWrite(SIM800_Trigger,LOW);
  return status;
}

//Post to give notification if rain is starting
int vPostDataRainingAlert() {
  char resource[128] = "/Ombrometer/Rain_alert/";
  sprintf(resource,"/Ombrometer/Rain_alert/%lu",chipID);
  Serial.println(resource);

  unsigned long time_out=millis();
  digitalWrite(SIM800_Trigger,HIGH);
  modem.init();

  String modemInfo = modem.getModemInfo();
  Serial.print(F("Modem: "));
  Serial.println(modemInfo);
  
  Serial.print("Waiting for network...");
  if (!modem.waitForNetwork() && (millis()-time_out>15000)) {
    Serial.println(" Fail");
    return 0;
  }
  Serial.println(" Success");
  if (modem.isNetworkConnected()) { Serial.println("Network connected"); }

  // GPRS connection parameters are usually set after network registration
  Serial.print(F("Connecting to "));
  Serial.print(apn);
  if (!modem.gprsConnect(apn, user, pass)) {
    Serial.println(" fail");
    return 0;
  }
  Serial.println(" success");
  if (modem.isGprsConnected()) { Serial.println("GPRS connected"); }

  now = myRTC.now();
  unsigned long unix=now.unixtime()-25200;

  const char contentType[64] = "application/x-www-form-urlencoded";
  char postData[128];
  sprintf(postData,"TimeAlert=%ld&RainStart=%d&RainStop=%d&RainRate=%f&Alert=1\0",unix,bRainStart,bRainStop,rainrate);
  http.connectionKeepAlive();  // Currently, this is needed for HTTPS
  http.post(resource, contentType, postData);

  // read the status code and body of the response
  int statusCode = http.responseStatusCode();
  String response = http.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);
  sscanf(response.c_str(),"%d,%f,%f,%d,%d",&period,&tipmm,&alert,&stopTime,&periodSD);

  if (period!=oldPeriod) {
    EEPROM.put(0,period);
    EEPROM.commit();
  }
  if (tipmm!=oldTipmm) {
    EEPROM.put(sizeof(period), tipmm);
    EEPROM.commit();
  }
  if (alert!=oldAlert) {
    EEPROM.put(sizeof(period)+sizeof(tipmm), alert);
    EEPROM.commit();
  }

  if (stopTime!=oldStopTime) {
    EEPROM.put(sizeof(period)+sizeof(tipmm)+sizeof(alert), stopTime);
    EEPROM.commit();
  }
  if (periodSD!=oldPeriodSD) {
    EEPROM.put(sizeof(period)+sizeof(tipmm)+sizeof(alert)+sizeof(stopTime), periodSD);
    EEPROM.commit();
  }
  
  oldPeriod=period;
  oldTipmm=tipmm;
  oldAlert=alert;
  oldStopTime=stopTime;
  oldPeriodSD=periodSD;
  
  digitalWrite(SIM800_Trigger,LOW);
  vTurnOffSD();
  return statusCode;
}


//Post to give notification if rain is starting
int vPostDataRaining() {
  char resource[128] = "/Ombrometer/Rain_start/";
  sprintf(resource,"/Ombrometer/Rain_start/%lu",chipID);
  Serial.println(resource);

  unsigned long time_out=millis();
  digitalWrite(SIM800_Trigger,HIGH);
  modem.init();

  String modemInfo = modem.getModemInfo();
  Serial.print(F("Modem: "));
  Serial.println(modemInfo);
  
  Serial.print("Waiting for network...");
  if (!modem.waitForNetwork() && (millis()-time_out>15000)) {
    Serial.println(" Fail");
    return 0;
  }
  Serial.println(" Success");
  if (modem.isNetworkConnected()) { Serial.println("Network connected"); }

  // GPRS connection parameters are usually set after network registration
  Serial.print(F("Connecting to "));
  Serial.print(apn);
  if (!modem.gprsConnect(apn, user, pass)) {
    Serial.println(" fail");
    return 0;
  }
  Serial.println(" success");
  if (modem.isGprsConnected()) { Serial.println("GPRS connected"); }

  now = myRTC.now();
  unsigned long unix=now.unixtime()-25200;

  const char contentType[64] = "application/x-www-form-urlencoded";
  char postData[128];
  sprintf(postData,"TimeRain=%ld&RainStart=%d&RainStop=%d&RainRate=%f&Alert=0\0",unix,bRainStart,bRainStop,rainrate);
  http.connectionKeepAlive();  // Currently, this is needed for HTTPS
  http.post(resource, contentType, postData);

  // read the status code and body of the response
  int statusCode = http.responseStatusCode();
  String response = http.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);
  sscanf(response.c_str(),"%d,%f,%f,%d,%d",&period,&tipmm,&alert,&stopTime,&periodSD);

  if (period!=oldPeriod) {
    EEPROM.put(0,period);
    EEPROM.commit();
  }
  if (tipmm!=oldTipmm) {
    EEPROM.put(sizeof(period), tipmm);
    EEPROM.commit();
  }
  if (alert!=oldAlert) {
    EEPROM.put(sizeof(period)+sizeof(tipmm), alert);
    EEPROM.commit();
  }

  if (stopTime!=oldStopTime) {
    EEPROM.put(sizeof(period)+sizeof(tipmm)+sizeof(alert), stopTime);
    EEPROM.commit();
  }
  if (periodSD!=oldPeriodSD) {
    EEPROM.put(sizeof(period)+sizeof(tipmm)+sizeof(alert)+sizeof(stopTime), periodSD);
    EEPROM.commit();
  }
  
  oldPeriod=period;
  oldTipmm=tipmm;
  oldAlert=alert;
  oldStopTime=stopTime;
  oldPeriodSD=periodSD;
  
  digitalWrite(SIM800_Trigger,LOW);
  return statusCode;
}

//Post to give notification if IO changed
int vPostDataIO() {
  char resource[128] = "/Ombrometer/IO/";
  sprintf(resource,"/Ombrometer/IO/%lu",chipID);
  Serial.println(resource);

  unsigned long time_out=millis();
  digitalWrite(SIM800_Trigger,HIGH);
  modem.init();

  String modemInfo = modem.getModemInfo();
  Serial.print(F("Modem: "));
  Serial.println(modemInfo);
  
  Serial.print("Waiting for network...");
  if (!modem.waitForNetwork() && (millis()-time_out>15000)) {
    Serial.println(" Fail");
    return 0;
  }
  Serial.println(" Success");
  if (modem.isNetworkConnected()) { Serial.println("Network connected"); }

  // GPRS connection parameters are usually set after network registration
  Serial.print(F("Connecting to "));
  Serial.print(apn);
  if (!modem.gprsConnect(apn, user, pass)) {
    Serial.println(" fail");
    return 0;
  }
  Serial.println(" success");
  if (modem.isGprsConnected()) { Serial.println("GPRS connected"); }

  now = myRTC.now();
  unsigned long unix=now.unixtime()-25200;
  
  const char contentType[64] = "application/x-www-form-urlencoded";
  char postData[128];
  sprintf(postData,"TimeIO=%ld&IO[0]=%d&IO[1]=%d&IO[2]=%d&IO[3]=%d&IO[4]=%d&IO[5]=%d&IO[6]=%d\0",unix,IO[0],IO[1],IO[2],IO[3],IO[4],IO[5],IO[6]);
  http.connectionKeepAlive();  // Currently, this is needed for HTTPS
  http.post(resource, contentType, postData);

  // read the status code and body of the response
  int statusCode = http.responseStatusCode();
  String response = http.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);

  sscanf(response.c_str(),"%d,%f,%f,%d,%d",&period,&tipmm,&alert,&stopTime,&periodSD);

  if (period!=oldPeriod) {
    EEPROM.put(0,period);
    EEPROM.commit();
  }
  if (tipmm!=oldTipmm) {
    EEPROM.put(sizeof(period), tipmm);
    EEPROM.commit();
  }
  if (alert!=oldAlert) {
    EEPROM.put(sizeof(period)+sizeof(tipmm), alert);
    EEPROM.commit();
  }

  if (stopTime!=oldStopTime) {
    EEPROM.put(sizeof(period)+sizeof(tipmm)+sizeof(alert), stopTime);
    EEPROM.commit();
  }
  if (periodSD!=oldPeriodSD) {
    EEPROM.put(sizeof(period)+sizeof(tipmm)+sizeof(alert)+sizeof(stopTime), periodSD);
    EEPROM.commit();
  }
  
  oldPeriod=period;
  oldTipmm=tipmm;
  oldAlert=alert;
  oldStopTime=stopTime;
  oldPeriodSD=periodSD;
  
  digitalWrite(SIM800_Trigger,LOW);
  return statusCode;
}


//Post to give notification if IO changed
int vPostLoRa() {
  char resource[128] = "/Ombrometer/IOLoRa/";
  sprintf(resource,"/Ombrometer/IOLoRa/%lu",chipID);
  Serial.println(resource);

  unsigned long time_out=millis();
  digitalWrite(SIM800_Trigger,HIGH);
  modem.init();

  String modemInfo = modem.getModemInfo();
  Serial.print(F("Modem: "));
  Serial.println(modemInfo);
  
  Serial.print("Waiting for network...");
  if (!modem.waitForNetwork() && (millis()-time_out>15000)) {
    Serial.println(" Fail");
    return 0;
  }
  Serial.println(" Success");
  if (modem.isNetworkConnected()) { Serial.println("Network connected"); }

  // GPRS connection parameters are usually set after network registration
  Serial.print(F("Connecting to "));
  Serial.print(apn);
  if (!modem.gprsConnect(apn, user, pass)) {
    Serial.println(" fail");
    return 0;
  }
  Serial.println(" success");
  if (modem.isGprsConnected()) { Serial.println("GPRS connected"); }

  now = myRTC.now();
  unsigned long unix=now.unixtime()-25200;
  
  const char contentType[64] = "application/x-www-form-urlencoded";
  char postData[128];
  sprintf(postData,"TimeIO=%ld&IOLoRa[0]=%s&IOLoRa[1]=%s&IOLoRa[2]=%s&IOLoRa[3]=%s&IOLoRa[4]=%s&IOLoRa[5]=%s&IOLoRa[6]=%s\0",unix,LoRaInput[0],LoRaInput[1],LoRaInput[2],LoRaInput[3],LoRaInput[4],LoRaInput[5],LoRaInput[6]);
//  Serial.println(LoRaInput[0]);
//  Serial.println(postData);
  http.connectionKeepAlive();  // Currently, this is needed for HTTPS
  http.post(resource, contentType, postData);

  // read the status code and body of the response
  int statusCode = http.responseStatusCode();
  String response = http.responseBody();

  Serial.print("Status code: ");
  Serial.println(statusCode);
  Serial.print("Response: ");
  Serial.println(response);

  sscanf(response.c_str(),"%d,%f,%f,%d,%d",&period,&tipmm,&alert,&stopTime,&periodSD);

  if (period!=oldPeriod) {
    EEPROM.put(0,period);
    EEPROM.commit();
  }
  if (tipmm!=oldTipmm) {
    EEPROM.put(sizeof(period), tipmm);
    EEPROM.commit();
  }
  if (alert!=oldAlert) {
    EEPROM.put(sizeof(period)+sizeof(tipmm), alert);
    EEPROM.commit();
  }

  if (stopTime!=oldStopTime) {
    EEPROM.put(sizeof(period)+sizeof(tipmm)+sizeof(alert), stopTime);
    EEPROM.commit();
  }
  if (periodSD!=oldPeriodSD) {
    EEPROM.put(sizeof(period)+sizeof(tipmm)+sizeof(alert)+sizeof(stopTime), periodSD);
    EEPROM.commit();
  }
  
  oldPeriod=period;
  oldTipmm=tipmm;
  oldAlert=alert;
  oldStopTime=stopTime;
  oldPeriodSD=periodSD;
  
  digitalWrite(SIM800_Trigger,LOW);
  return statusCode;
}
int getGSMTIME()
{
    Serial.printf("GSM Time: %s\n", modem.getGSMDateTime(DATE_FULL).c_str());
    int     GSMyear = 0;
    int     GSMmonth = 0;
    int     GSMdate = 0;
    int     GSMhours = 0;
    int     GSMminutes = 0;
    int     GSMseconds = 0;
    float   GSMtimezone = 0;
    time_t  GSMUTCtime = 0;

    if (modem.getNetworkTime(&GSMyear, &GSMmonth, &GSMdate, &GSMhours, &GSMminutes, &GSMseconds, &GSMtimezone))
    {
      struct tm s;
      s.tm_sec  = (GSMseconds);
      s.tm_min  = (GSMminutes);
      s.tm_hour = (GSMhours);
      s.tm_mday = (GSMdate);
      s.tm_mon  = (GSMmonth - 1);
      s.tm_year = (GSMyear - 1900);
      GSMUTCtime   = mktime(&s);
      Serial.printf("GSM Time:    %s",  ctime(&GSMUTCtime));
      if (GSMUTCtime > 1615155060) {      //  check for valid time, not 1939!
        clocks.setYear(GSMyear-2000);
        clocks.setMonth(GSMmonth);
        clocks.setDate(GSMdate);
        clocks.setHour(GSMhours);
        clocks.setMinute(GSMminutes);
        clocks.setSecond(GSMseconds);
        return 1;
      }
    }
    return 0;
}
