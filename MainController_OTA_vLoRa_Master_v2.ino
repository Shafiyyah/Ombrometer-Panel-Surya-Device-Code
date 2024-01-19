#define oldVersion 6

#define SERVER_ADDRESS 8

const char CLIENT_ADDRESS[7]={0x1,0x2,0x3,0x4,0x5,0x6,0x7};

#define CHUNCK_SIZE 64
#define LCD_BUT 27
#define LCD_Trigger 32
#define SIM800_Trigger 33
#define SD_Trigger 25
#define HALL_SENSOR 36
#define SDMOSI 23
#define SDMISO 19
#define SDCLK 18
#define SDCS 5
#define BUZZER 26
#define BATTERY 39
#define SIMRST 4
#define SIMRX 17
#define SIMTX 16
#define PCF_INT 13
#define LORA_NSS 15
#define LORA_DI0 2

RTC_DATA_ATTR char IO[7];
//char IOUpload[7];
RTC_DATA_ATTR char oldIO[7]={'1','1','1','1','1','1','1'};
RTC_DATA_ATTR char oldIOBuzzer[7]={'1','1','1','1','1','1','1'};
RTC_DATA_ATTR int bootCount=0;;
RTC_DATA_ATTR int bootCountSD=0;;

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27,20,4); 

#include <DS3231.h>

RTClib myRTC;
DS3231 clocks;

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  10        /* Time ESP32 will go to sleep (in seconds) */


#include "EEPROM.h"
#define EEPROM_SIZE 256

int period=60;
float tipmm=0;
float alert=0;
int stopTime=0;
int periodSD=10;
int oldPeriod=60;
float oldTipmm=0;
float oldAlert=0;
int oldStopTime=0;
int oldPeriodSD=10;
int signalSIM800=-1;
int resetDevice=0;


RTC_DATA_ATTR bool bRainStart=0;
RTC_DATA_ATTR bool bRainStop=0;
RTC_DATA_ATTR bool bAlert;
RTC_DATA_ATTR bool bAlerted=0;
RTC_DATA_ATTR bool bPeriodicOn=0;
RTC_DATA_ATTR bool bOTAStart=0;

int newVersion=0;

#define TINY_GSM_MODEM_SIM800

#define SerialAT Serial2

#if !defined(TINY_GSM_RX_BUFFER)
#define TINY_GSM_RX_BUFFER 1024
#endif

// See all AT commands, if wanted
// #define DUMP_AT_COMMANDS

#define TINY_GSM_DEBUG Serial
// #define LOGGING  // <- Logging is for the HTTP library

#define TINY_GSM_USE_GPRS true

// Your GPRS credentials, if any
const char apn[]  = "internet";
const char user[] = "";
const char pass[] = "";
const char server[]= "asia-east1-ombrometerlora.cloudfunctions.net";

const int  port = 443;

// Server details
const char serverOTA[]   = "firebasestorage.googleapis.com";
const char resourceOTA[] = "/v0/b/ombrometerlora.appspot.com/o/MainController_OTA_vLoRa_Master_v2.ino.esp32.bin?alt=media";
const int  portOTA       = 443;

#include <TinyGsmClient.h>
#include <ArduinoHttpClient.h>


#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, Serial);
TinyGsm        modem(debugger);
#else
TinyGsm        modem(SerialAT);
#endif

TinyGsmClientSecure client(modem);
HttpClient          httpOTA(client, serverOTA, portOTA);
HttpClient          http(client, server, port);

#include "FS.h"
#include "SPIFFS.h"
#include <CRC32.h>
#include <MD5Builder.h>
#include <Update.h>

//SD Card Library
#include "FS.h"
#include "SD.h"
#include "SPI.h"



uint64_t chipID = 0;

RTC_DATA_ATTR double rainrate;
RTC_DATA_ATTR double oldRainrate;
RTC_DATA_ATTR double rainrateDay;
RTC_DATA_ATTR int last_tick;

void IRAM_ATTR isr() {
  rainrate+=tipmm;
  rainrateDay+=tipmm;
  last_tick=0;
}

void TaskBuzzer( void *pvParameters );

esp_sleep_wakeup_cause_t wakeup_reason;

float vBatt=0;
RTC_DATA_ATTR int oldDay=0;
RTC_DATA_ATTR bool bSendSD=0;
DateTime now;

#include <Adafruit_PCF8574.h>
Adafruit_PCF8574 pcf;

const int IOPIN[7] = {0,1,2,3,4,5,6};
int bDoneTask=0;
int bDoneLoRa=0;

#define RH_MESH_MAX_MESSAGE_LEN 50

#include <RHMesh.h>
#include <RH_RF95.h>
#include <SPI.h>

// Singleton instance of the radio driver
RH_RF95 driver(LORA_NSS,LORA_DI0);

// Class to manage message delivery and receipt, using the driver declared above
RHMesh manager(driver, SERVER_ADDRESS);

#define LORA_PIN_BITMASK 0x000000004

char LoRaIO[7][6];
RTC_DATA_ATTR char oldLoRaIO[7][6];

RTC_DATA_ATTR char LoRaInput[7][6];
RTC_DATA_ATTR char oldLoRaInput[7][6];

#define RECEIVER_MODE 1
#define TRANSMIT_MODE 0
bool LoRaMode=RECEIVER_MODE;

RTC_DATA_ATTR bool bSendLoRaData=0;

void setup() {
  Serial.begin(115200);
  SerialAT.begin(19200);
  Serial.print("Firmware Version: ");
  Serial.println(oldVersion);
//  Serial.println("OTA SUCCEEDED THIS IS NEXT VERSION");

  pinMode(HALL_SENSOR,INPUT);
//  pinMode(PCF_INT,INPUT);
  attachInterrupt(HALL_SENSOR, isr, RISING);


  //Get Device ID
  chipID=ESP.getEfuseMac();

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0, period);
  EEPROM.get(sizeof(period), tipmm);
  EEPROM.get(sizeof(period)+sizeof(tipmm), alert);
  EEPROM.get(sizeof(period)+sizeof(tipmm)+sizeof(alert), stopTime);
  EEPROM.get(sizeof(period)+sizeof(tipmm)+sizeof(alert)+sizeof(stopTime), periodSD);
  EEPROM.get(sizeof(period)+sizeof(tipmm)+sizeof(alert)+sizeof(stopTime)+sizeof(periodSD), oldDay);
  oldPeriod=period;
  oldTipmm=tipmm;
  oldAlert=alert;
  oldStopTime=stopTime;
  oldPeriodSD=periodSD;

  
  
  print_wakeup_reason();

  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);

  esp_sleep_enable_ext0_wakeup((gpio_num_t)HALL_SENSOR,0); //1 = High, 0 = Low //Check Sensor

  esp_sleep_enable_ext1_wakeup(LORA_PIN_BITMASK,ESP_EXT1_WAKEUP_ANY_HIGH);

  pinMode(LCD_BUT,INPUT_PULLUP);
  pinMode(HALL_SENSOR,INPUT);
  pinMode(BATTERY,INPUT);

  pinMode(LCD_Trigger,OUTPUT);
  pinMode(SIM800_Trigger,OUTPUT);
  pinMode(SD_Trigger,OUTPUT);
  pinMode(BUZZER,OUTPUT);

  
  digitalWrite(LCD_Trigger,HIGH);
  gpio_hold_en((gpio_num_t)LCD_Trigger);

  vBatt=analogRead(BATTERY)*10.56*1.072/4096;
  gpio_deep_sleep_hold_en();

  Wire.begin();

  now = myRTC.now();
  

  if (!pcf.begin(0x20, &Wire)) {
    Serial.println("Couldn't find PCF8574");
  }
  for (uint8_t p=0; p<8; p++) {
    pcf.pinMode(p, INPUT_PULLUP);
  }

  pinMode(SDCS,OUTPUT);
  digitalWrite(SDCS,HIGH);
  if (!manager.init())
    Serial.println("RF95 init failed");
  // Defaults after init are 434.0MHz, 0.05MHz AFC pull-in, modulation FSK_Rb2_4Fd36
  driver.setFrequency(915.0);
  driver.setTxPower(20, false);
  
  lcd.init();
  lcd.noBacklight();
  lcd.setCursor(0,0);
  lcd.printf("Curah: %.2f mm",rainrate);
  lcd.setCursor(0,1);
  lcd.printf("Crh/Hari: %.2f mm",rainrateDay);
  lcd.setCursor(0,2);
  lcd.printf("Batre: %.2fV",vBatt);

  xTaskCreatePinnedToCore(
    TaskBuzzer
    ,  "TaskBuzzer"
    ,  4096  // Stack size
    ,  NULL  // When no parameter is used, simply pass NULL
    ,  2  // Priority
    ,  NULL // With task handle we will be able to manipulate with this task.
    ,  0 // Core on which the task will run
    );

  xTaskCreatePinnedToCore(
    TaskSendData
    ,  "TaskSendData"
    ,  20480  // Stack size
    ,  NULL  // When no parameter is used, simply pass NULL
    ,  3  // Priority
    ,  NULL // With task handle we will be able to manipulate with this task.
    ,  0 // Core on which the task will run
    );

  xTaskCreatePinnedToCore(
    TaskLoRa
    ,  "TaskLoRa"
    ,  20480  // Stack size
    ,  NULL  // When no parameter is used, simply pass NULL
    ,  3  // Priority
    ,  NULL // With task handle we will be able to manipulate with this task.
    ,  1 // Core on which the task will run
    );
  

}


RTC_DATA_ATTR int OTAretry=0;


void loop() {
  //Nothing Here
}

void TaskSendData(void *pvParameters){  // This is a task.
  (void) pvParameters;
  
  for (;;){ // A Task shall never return or exit.
    //Check the need for OTA
    if (bOTAStart) {
      vUpdateOTA();
      OTAretry++;
      if (OTAretry>2) {
        bOTAStart=0;
      }
      digitalWrite(SIM800_Trigger,LOW);
    }
    
    //Send SD Card File everyday
    if (bSendSD) {
      rainrateDay=0;
      if(vPostDataSDCard()==200) {
        bSendSD=0;
        oldDay=now.day();
        EEPROM.put(sizeof(period)+sizeof(tipmm)+sizeof(alert)+sizeof(stopTime)+sizeof(periodSD), oldDay);
        EEPROM.commit();
      } else if (vPostDataSDCard()==0) {
        bSendSD=0;
        oldDay=now.day();
        EEPROM.put(sizeof(period)+sizeof(tipmm)+sizeof(alert)+sizeof(stopTime)+sizeof(periodSD), oldDay);
        EEPROM.commit();
      }
    }

    if (bSendLoRaData) {
      if (vPostLoRa() == 200) {
        bSendLoRaData=0;
        for (int i=0; i<7; i++) {
          strncpy(oldLoRaInput[i],LoRaInput[i],5);
        }
      }
    }
    
    //Send Periodic on 1 st time on
    if (!bPeriodicOn) {
      if (vPostDataPeriodic()==200) {
        bPeriodicOn=1;
      }
    }
    
    //Wake up by timer
    if (wakeup_reason==ESP_SLEEP_WAKEUP_TIMER) {
      //Check IO
      vCheckIO();
  //    for (int i=0; i<7;i++) {
  //      if (pcf.digitalRead(IOPIN[i])) {
  //        IOUpload[i]='1';
  //      } else {
  //        IOUpload[i]='0';
  //      }
  //      
  //    }
      bootCount++;
      last_tick++;
      bootCountSD++;
  
     
      
      Serial.print("COMPARE:");
      Serial.print(IO);
      Serial.print(",");
      Serial.print(oldIO);
      Serial.print(",");
      Serial.println(strncmp(IO,oldIO,7));
      if (strncmp(IO,oldIO,7)) {
        if (vPostDataIO()==200) {
          Serial.println("UPDATE IO SUCCESS");
          strncpy(oldIO,IO,7);
          esp_deep_sleep_start();
        }
      }
  
      //Send RainStop if didn't tick after stopTime and rain has started
      if ((bRainStart)&&(last_tick>=stopTime)) {
        bRainStop=1;
        oldRainrate=rainrate;
        Serial.println("STOP RAIN");
  
        //Send Data Rain Stop
        if(vPostDataPeriodic()==200) {
          bootCount=0;
          if (bRainStop) {
            bRainStart=0;
            bAlert=0;
            rainrate=0;
          }
         //You are done here, Sleep 
         Serial.println("SLEEP");
         while(!bDoneTask || !bDoneLoRa){
          vTaskDelay(1);
  //        Serial.println(bDoneTask);
         }
          esp_deep_sleep_start();
        }
      }
  
      
      //Send Data Periodically
      if (bootCount>=period) {
        if(vPostDataPeriodic()==200) {
          bootCount=0;
          if (bRainStop) {
            bRainStart=0;
            bAlert=0;
            bAlerted=0;
            rainrate=0;
          }
         //You are done here, Sleep 
          esp_deep_sleep_start();
        }
      }
  
      //Save Data on SD Card
      if (bootCountSD>=periodSD) {
        vSetupSDCard();
        char bufferTitle[64]="";
        now = myRTC.now();
        sprintf(bufferTitle, "/Ombrometer_%d-%d-%d.csv",now.year(),now.month(),now.day());
        char bufferData[256]="";
        sprintf(bufferData, "%d-%d-%d %d:%d:%d,%.2f,%.2f,%d,%d,%d,%d,%d,%d,%d\n",now.year(),now.month(),now.day(),now.hour(),now.minute(),now.second(),rainrate,rainrateDay,IO[0],IO[1],IO[2],IO[3],IO[4],IO[5],IO[6]);
        if (readFile(SD,bufferTitle)) {
          appendFile(SD,bufferTitle,bufferData);
          vTurnOffSD();
        } else {
          writeFile(SD,bufferTitle,"Time,Rain Rate, Rain Rate (Daily),IO[1],IO[2],IO[3],IO[4],IO[5],IO[6],IO[7]\n");
          appendFile(SD,bufferTitle,bufferData);
          vTurnOffSD();
        }
        bootCountSD=0;
        Serial.println("SLEEP");
          while(!bDoneTask|| !bDoneLoRa){
            vTaskDelay(1);
  //        Serial.println(bDoneTask);
          }
        esp_deep_sleep_start();
      }
      Serial.println("SLEEP");
          while(!bDoneTask || !bDoneLoRa){
            vTaskDelay(1);
  //        Serial.println(bDoneTask);
          }
      esp_deep_sleep_start();
    }
    
    //Wake up by hall Sensor 
    else if (wakeup_reason==ESP_SLEEP_WAKEUP_EXT0) {
      last_tick=0;
      if (!bRainStart) {
        bRainStop=0;
        bRainStart=1;
        if(vPostDataRaining()==200) {
          esp_deep_sleep_start();
        }
      }
      if((rainrate>=alert)&&(!bAlerted)) {
        if(vPostDataRainingAlert()==200) {
          bAlerted=1;
          esp_deep_sleep_start();
        }
      }
      Serial.println("SLEEP");
          while(!bDoneTask || !bDoneLoRa){
            vTaskDelay(1);
  //        Serial.println(bDoneTask);
          }
      esp_deep_sleep_start();
    } else {
      Serial.println("SLEEP");
          while(!bDoneTask || !bDoneLoRa){
            vTaskDelay(1);
  //        Serial.println(bDoneTask);
          }
      esp_deep_sleep_start();
    }

    vTaskDelay(100);
  }
}

void TaskBuzzer(void *pvParameters){  // This is a task.
  (void) pvParameters;
  
  for (;;){ // A Task shall never return or exit.
    vCheckIO();
    int waterLevel=0;
    //if oldIO and IO are different Send to Server
    //    Serial.println(strcmp(IO,oldIO));
    //    Serial.println("BUZZER");
//    Serial.println(IO);
      if (strncmp(IO,oldIOBuzzer,7)) {
        for (int i=0; i<7;i++) {
    //        if (IO[i]!=oldIO[i]) {
            
            if (IO[i]=='0') {
              waterLevel=i;
    //            delay(2000);
            }
    //        }
        }
        Serial.println(waterLevel);
        for (int j=0;j<=waterLevel;j++) {
          digitalWrite(BUZZER,HIGH);
          vTaskDelay(300);
          digitalWrite(BUZZER,LOW);
          vTaskDelay(300);
          Serial.println(j);
        }
        strncpy(oldIOBuzzer,IO,7);
      }
      bDoneTask=1;
      vTaskDelay(30);
  }
}


void TaskLoRa(void *pvParameters){  // This is a task.
  (void) pvParameters;

  int retry=0;
  for (;;){ // A Task shall never return or exit.
    uint8_t data[] = "Data Received by Server";
    // Dont put this on the stack:
    uint8_t buf[RH_MESH_MAX_MESSAGE_LEN];
    
    uint8_t len = sizeof(buf);
    uint8_t from;

    if (LoRaMode==RECEIVER_MODE) {
      if (manager.recvfromAck(buf, &len, &from))
      {
        Serial.print("got request from : 0x");
        Serial.print(from, HEX);
        Serial.print(": ");
        Serial.println((char*)buf);

        strncpy(LoRaInput[from-1],(char*)buf,5);
        Serial.println(LoRaInput[from-1]);
        if (strcmp(LoRaInput[from-1],oldLoRaInput[from-1])) {
          bSendLoRaData=1;
        }
    
        // Send a reply back to the originator client
        if (manager.sendtoWait(data, sizeof(data), from) != RH_ROUTER_ERROR_NONE) {
          Serial.println("sendtoWait failed");
        } else {
          bDoneLoRa=1;
        }
      }
       //Go to sleep now
      if (millis()>10000) {
    //       Serial.println("Going to sleep now");
         bDoneLoRa=1;
    //      esp_deep_sleep_start();
      }
    } else {
      LoRaMode=RECEIVER_MODE;
      bDoneLoRa=1;
//      for (int i=0; i<7; i++) {
//        if (strcmp(LoRaIO[i],oldLoRaIO[i])) {
//          LoRaMode=TRANSMIT_MODE;
//          bDoneLoRa=0;
//        }
//      }
      
    }
   
    vTaskDelay(30);
  }
}

//Check IO State
void vCheckIO() {
  for (int i=0; i<7;i++) {
    if (pcf.digitalRead(IOPIN[i])) {
      IO[i]='1';
    } else {
      IO[i]='0';
    }
    
  }
}



void print_wakeup_reason(){
  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); rainrate+=tipmm; rainrateDay+=tipmm; break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}
