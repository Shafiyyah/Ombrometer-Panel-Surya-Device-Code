void vUpdateOTA() {
  
  uint32_t length = 0;
  bool errorDownload = 1;
  bool bSPIFFSUsed=1;
  for (int xj = 0; xj < 3 && errorDownload; xj++) {
    bool SPIFFSSucceeds = 0;
    if(bSPIFFSUsed){
      Serial.println(F("SPIFFS Format..."));
      for (int xi = 0; xi < 3 && !SPIFFS.format(); xi++){
        Serial.println(F("Format Failed..."));
      }
      for (int xi = 0; xi < 3 && !SPIFFSSucceeds; xi++){
        SPIFFSSucceeds = SPIFFS.begin();
        if (!SPIFFSSucceeds) {
          Serial.println("SPIFFS Mount Failed");
          Serial.println(F("SPIFFS Format..."));
          for (int xk = 0; xk < 3 && !SPIFFS.format(); xk++){
            Serial.println(F("Format Failed..."));
          }
        }
      }
      if (!SPIFFSSucceeds) {
        return;
      }
    }
    bSPIFFSUsed=0;
    errorDownload = 0;
    digitalWrite(SIM800_Trigger,HIGH);
    Serial.print(F("Waiting for network..."));
    modem.restart();
    int i;
    for (i = 0; i < 3 && !modem.waitForNetwork(60000L, false); i++) {
      Serial.println(F("Restarting modem..."));
      modem.restart();
    }
    if (i == 3) {
      Serial.println(F("Network failed and cancel update OTA"));
      errorDownload = 1;
      xj = 3;
    }
    else {
      Serial.println(" Network OK");
      Serial.print(F("Connecting to "));
      Serial.print(apn);
      if (!modem.gprsConnect(apn, user, pass)) {
        Serial.println(" GPRS Connection failed");
        errorDownload = 1;
      }
      else {
        if (modem.isGprsConnected()) { Serial.println(" GPRS connected"); }
        
        httpOTA.connectionKeepAlive();  // Currently, this is needed for HTTPS
        int err = httpOTA.get(resourceOTA);
        if (err != 0) {
          Serial.println(F("failed to connect"));
          errorDownload=1;
        } else {
          int status = httpOTA.responseStatusCode();
          Serial.print(F("Response status code: "));
          Serial.println(status);
          if (!status) {
            errorDownload=1;
          }


          unsigned long timeout = millis();
          
          if (!errorDownload) {
            Serial.println(F("Response Headers:"));
            while (httpOTA.headerAvailable()) {
              String headerName  = httpOTA.readHeaderName();
              String headerValue = httpOTA.readHeaderValue();
              Serial.println("    " + headerName + " : " + headerValue);
            }
          
            length = httpOTA.contentLength();
            if (length >= 0) {
              Serial.print(F("Content length is: "));
              Serial.println(length);
              Serial.println(F("Reading response data"));
//              timeout = millis();
              CRC32 crc;
              MD5Builder md5;
              md5.begin();
              uint32_t readLength = 0;

              unsigned long timeElapsed = millis();
              printPercent(readLength, length);
              File file = SPIFFS.open("/update.bin", "w+");
              while (readLength < length && client.connected() && !errorDownload /*&& millis() - timeout < 10000L*/) {
                int i = 0;
                while (client.available() && !errorDownload) {
                  uint8_t c = client.read();
                  //                  Serial.print((char)c);       // Uncomment this to show data
                  if (!file.print(char(c)))
                  {
                    Serial.println("error_writing_character_to_SPIFFS");
                    errorDownload = 1;
                  }
                  crc.update(c);
                  md5.add(String(c));
                  readLength++;
                  if (readLength % (length / 13) == 0) {
                    printPercent(readLength, length);
                  }
                }
              }
              bSPIFFSUsed=1;
              printPercent(readLength, length);
              if (readLength < length) {
                errorDownload = 1;
                Serial.println(F("File Downloaded not Complete..."));
              }
              else {
                
                uint32_t CRCResult = crc.finalize();
                char CRCResultChar[12];
                char CRC32Web[12];
                String tempString=String(CRCResult,HEX);
                tempString.toCharArray(CRCResultChar,12);
                //  sprintf(CRCResultChar, "%X%08", CRCResult);
                
                Serial.print("Calc. CRC32:    toCharArray 0x"); Serial.println(CRCResultChar);
                md5.calculate();
                Serial.print("Calc. MD5:    "); Serial.println(md5.toString());
                strcpy(CRC32Web,CRCResultChar);
                if(strcmp(CRCResultChar,CRC32Web)==0){ //return 0 if strings are equal
                  timeElapsed = millis() - timeElapsed;
                  float duration = float(timeElapsed) / 1000;
                  Serial.println();
                  Serial.print("Content-Length: ");   Serial.println(length);
                  Serial.print("Actually read:  ");   Serial.println(readLength);
                  Serial.print("Duration:       ");   Serial.print(duration); Serial.println("s");
                }
                else{
                  errorDownload = 1;
                  Serial.println(F("CRC32 Failed..."));
                }
              }
              file.close();
            }
            else {
              errorDownload = 1;
            }
          }
        }
        // Shutdown
        httpOTA.stop();
        Serial.println(F("Server disconnected"));
        
      }
      modem.gprsDisconnect();
      Serial.println(F("GPRS disconnected"));
      // AT+SAPBR=0,1
      delay(1000);
      Serial.println("AT+HTTPTERM");
      SerialAT.println("AT+HTTPTERM");
      delay(500); 
      Serial.println("AT+SAPBR=0,1"); 
      SerialAT.println("AT+SAPBR=0,1");
      delay(1000); 
    }
  }
  if (!errorDownload) {
    Serial.println("starting_Update");
    updateFromFS(length);
  }
  else{
//    vDeepSleepSaveData();
  }
  digitalWrite(SIM800_Trigger,LOW);
}


void updateFromFS(uint32_t length){
  File updateBin = SPIFFS.open("/update.bin", "r");
  if (updateBin)
  {
    size_t updateSize = updateBin.size();
    if (length == updateSize) {
      if (updateSize > 0)
      {
        Serial.println("start_of_updating");
        performUpdate(updateBin, updateSize);
      }
      else
      {
        Serial.println("Error,_file_is_empty");
      }
    }
    else {
      Serial.println("Error,_file_is_corrupted");
    }

    updateBin.close();
  }
  else
  {
    Serial.println("can't_open_update.bin");
  }
}


void performUpdate(Stream &updateSource, size_t updateSize){
  bool bProgramUpdated=0;
  if (Update.begin(updateSize))
  {
    size_t written = Update.writeStream(updateSource);
    if (written == updateSize)
    {
      Serial.println("writings:_" + String(written) + "_successfully");
    }
    else
    {
      Serial.println("only writing:_" + String(written) + "/" + String(updateSize) + ". Retry?");
    }
    if (Update.end())
    {
      Serial.println("OTA_accomplished!");
      if (Update.isFinished())
      {
        Serial.println("OTA_ended_restarting!");
        bProgramUpdated = 1;
//        vUpdateProgramUpdatedEEPROM();
        ESP.restart();
      }
      else
      {
        Serial.println("OTA_didn't_finish?_something_went_wrong!");
      }
    }
    else
    {
      Serial.println("Error_occured_#:_" + String(Update.getError()));
    }
  }
  else
  {
    Serial.println("without_enough_space_to_do_OTA");
  }
}

void printPercent(uint32_t readLength, uint32_t length) {
//   If we know the total length
  if (length != -1) {
    Serial.print("\r ");
    Serial.print((100.0 * readLength) / length);
    Serial.print('%');
  } else {
    Serial.println(readLength);
  }
}
