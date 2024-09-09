#include <Arduino.h>
#include <WiFiManager.h>

#include <rom/rtc.h>
#include <esp_log.h>
#include "esp32-hal-log.h" 
#include <WiFiClientSecure.h>
#include "sensorsConf.h"
#include <numeric>
#include <WiFi.h>

#include "BParasite.h"

//log all my code with high verbosity, except BParasite 
//because there's too much Bluetooth stuff happening at any moment
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <Logger/Logger.h>
#include <MaaajaaaClient/MaaajaaaClient.h>

const int scanTime = 5; // BLE scan time in seconds 

#define AP_NAME "MaaajaaaSensor"
#define DEFAULT_AP_PW "maaajaaa"

WiFiManager wifiManager;

void configModeCallback (WiFiManager *myWiFiManager);

void serialPrintNewReading(BParasite_Data_S *data, int i);

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

BParasite parasite(knownBLEAddresses, plantNames);
static const int plantNumer = knownBLEAddresses.size();
std::vector<BParasite_Data_S> parasiteData(NUMBER_OF_PLANTS);

SemaphoreHandle_t mutex;

//only for the flashing, which is intended to also work offline
std::vector<bool> warningNOTDelivered(NUMBER_OF_PLANTS);
std::vector<bool> criticalWarningNOTDelivered(NUMBER_OF_PLANTS);
std::vector<bool> offlineWarningNOTDelivered(NUMBER_OF_PLANTS);
std::vector<bool> moistureLow(NUMBER_OF_PLANTS);
std::vector<bool> moistureCritical(NUMBER_OF_PLANTS);

std::vector<time_t> lastTimeDataReceived(NUMBER_OF_PLANTS);
time_t lastTimeoutCheck = 0;
bool startedTimeout = false;

void connectToWifiAndGetDST();
void parasiteReadingTask(void *pvParameters);
void blink(int ,int);
Logger logger;
//static const char* TAG = "main";

MaaajaaaClient maaajaaaClient;

void setup() {
    Serial.begin(115200);
    //give the console some time, else we'll never see the first few seconds of messages  
    delay(2000);
    //initialize the logger first so we can first and foremost log (even if the time might be wrong for now)
    logger.begin();

    //set ESP logging to log to file (and still print as well)
    esp_log_set_vprintf(logger.logError);

    //let's just log this, yes the time will be near 0 but we might be able to detect bootloops

    //find out and log reason for restart
    int resetCauseCore1 =  rtc_get_reset_reason(0);
    int resetCauseCore2 = rtc_get_reset_reason(1);
    ESP_LOGD(myTAG,"REBOOT, cause core1: %i   cause core2:%i", resetCauseCore1, resetCauseCore2);


    //now let's connect to WiFi to get the time
    connectToWifiAndGetDST();
    //only now it really makes sense to start reading from our sensor(s)
    // Initialization
    parasite.begin();

    //initialize mutex semaphore
    mutex = xSemaphoreCreateMutex();
    //create scanning task
    xTaskCreate(parasiteReadingTask,   "parasiteReadingTask",      5000,  NULL,        1,   NULL);
}

void loop() {  
  for (int i=0; i < NUMBER_OF_PLANTS; i++) { 

      // gets a local copy of the ith sensor reading
      // mutex protection ensures reading struct integrity
      BParasite_Data_S prstDatCpyAtIndexI;
      if(mutex != NULL){
          if (xSemaphoreTake(mutex, (TickType_t)10)==pdTRUE) {
          prstDatCpyAtIndexI = parasiteData[i];
          xSemaphoreGive(mutex);
        } else prstDatCpyAtIndexI.valid = false;
      } else{
          prstDatCpyAtIndexI.valid = false;
          Serial.println("Mutex null");
      }


      //valid means new data and no error in this case
      //everything will only be processed while the data is fresh here
      if (prstDatCpyAtIndexI.valid) {
          serialPrintNewReading(&prstDatCpyAtIndexI, i);
      }

      //invalid data won't be proccessed unless a message needs to be resent
      if (prstDatCpyAtIndexI.valid || warningNOTDelivered[i] || criticalWarningNOTDelivered[i]){
          //reset offline warning
          offlineWarningNOTDelivered[i] = false;
          
          //send entwarnung if sensor has been offline for too long (as it's reasonable to assume that a warning had been sent)
          if(time(nullptr)-lastTimeDataReceived[i] >= OFFLINE_WARNING_TIME * 60 && lastTimeDataReceived[i] != 0){
            criticalWarningNOTDelivered[i]=false;
            //messenger.sendOfflineEntwarnung(prstDatCpyAtIndexI);
          }

          if (prstDatCpyAtIndexI.valid){   
            //update the last time it received data
            lastTimeDataReceived[i] = time(nullptr);
          }
          
          if( prstDatCpyAtIndexI.soil_moisture/100.0 <= CRITICAL_WARNING_LEVEL){
            warningNOTDelivered[i]=false;
            moistureLow[i]=true;
            if(!moistureCritical[i]){
              criticalWarningNOTDelivered[i]=true;
              moistureCritical[i]=true;
            }
            if(criticalWarningNOTDelivered[i]){
              criticalWarningNOTDelivered[i] = false;//! messenger.sendCriticallyLowMessage(prstDatCpyAtIndexI);
            }
            
          }else if( prstDatCpyAtIndexI.soil_moisture/100.0 <= LOW_MOISTURE_LEVEL){
            //LOW MOISTURE
            Serial.println("LOW MOISTURE");
            moistureCritical[i] = false;
            //detect dropping peak
            if(!moistureLow[i]){
              moistureLow[i] = true;
              warningNOTDelivered[i] = true;
            }
            //deliver warning if neccessarry and connected
            if(warningNOTDelivered[i] && WiFi.status() == WL_CONNECTED){
              warningNOTDelivered[i] = false;//! messenger.sendLowMessage(prstDatCpyAtIndexI);
            }
          } else if( prstDatCpyAtIndexI.soil_moisture/100.0 >= WATERING_THANKYOU_LEVEL && (moistureLow[i] || moistureCritical[i])){
            //reset all warnings
            moistureCritical[i] = false;
            moistureLow[i] = false;
            warningNOTDelivered[i]=false;
            criticalWarningNOTDelivered[i]=false;
            //messenger.sendThankYouMessage(prstDatCpyAtIndexI);
          }

          //set data as invalid to signify that it has been processed
          if(mutex != NULL){
            if (xSemaphoreTake(mutex, (TickType_t)10)==pdTRUE) {
              parasiteData[i].valid = false;
              xSemaphoreGive(mutex);
            }
          } else{
            Serial.println("Mutex null");
          }
        }
  }
  
  //LED STUFF
  //at least one with low moisture and none with failed to send warning
  bool mstLow  = ( std::accumulate(moistureLow.begin(), moistureLow.end(),0) >= 1);
  bool mstCritLow = (std::accumulate(moistureCritical.begin(), moistureCritical.end(),0) >= 1);
  bool warnNDel = (std::accumulate(warningNOTDelivered.begin(), warningNOTDelivered.end(), 0) >= 1);
  bool critWarnNDel = (std::accumulate(criticalWarningNOTDelivered.begin(), criticalWarningNOTDelivered.end(), 0) >= 1);
  if(warnNDel || critWarnNDel){
    //if the delivery failed we'll wait for 2 minutes (which means retries) and then reconnect wifi
    if(!startedTimeout){
      lastTimeoutCheck = time(nullptr);
    }else{
      if(time(nullptr) - lastTimeoutCheck > TIME_BEFORE_RECONNECT){
        startedTimeout = false;
        //give it time to reconnect so the timeout check doesn't loop
        lastTimeoutCheck = time(nullptr) - WIFI_CONNECTION_WAIT;
        WiFi.disconnect();
        connectToWifiAndGetDST();
      }
    }
  }
  if(mstLow && !warnNDel){
    blink(200,200);
  //at least one with low moisture and at least with failed to send warning (which do not have to be the same ones but supposedly should, unless bug)
  }else if(mstLow && (warnNDel || critWarnNDel)){
    blink(500, 500);
  }else if(mstCritLow){
    blink(100,200);
  }else{
    //readings okay
    delay(1400);
  }
}

void parasiteReadingTask(void *pvParameters) {
  while (1) {
    //time that is maximally spent between scans/loop runs
    int cycleTime = 1000;
    parasite.resetData(); // Set sensor data invalid
    parasite.getData(5); // get sensor data (run BLE scan for 5 seconds)
    // makes a copy of each sensor reading under mutex protection
    for (int i=0; i < NUMBER_OF_PLANTS; i++){
      bool dataSaved = false;
      if(mutex != NULL){
        while(!dataSaved && cycleTime >= 100){
          //try to get mutex to write data
          if (xSemaphoreTake(mutex, (TickType_t)10)==pdTRUE) {
            //only update data (including the validity) if temperature, soil moisture or humidity have changed
            if(parasiteData[i].temperature != parasite.data[i].temperature 
                || parasiteData[i].soil_moisture != parasite.data[i].soil_moisture 
                || parasiteData[i].humidity != parasite.data[i].humidity){
                  parasiteData[i]=parasite.data[i];
                  bool logged = false;
                  time_t logTime = time(nullptr);
                  if(maaajaaaClient.logReading(parasiteData[i], knownBLEAddresses[i].c_str(), String(logTime)) == 201){
                    logged = true;
                  }
                  if(!logged){
                    //only cache if sending data to server failed
                    logger.cacheData(knownBLEAddresses[i].c_str(), parasiteData[i], logTime); 
                  }
            }
            xSemaphoreGive(mutex);
            dataSaved = true;
          }else{
            //if mutex fails try again in 10ms
            delay(10);
            cycleTime -= 10;
          }
        }        
      } else{
           Serial.println("Mutex null");
      }
    } 
    parasite.clearScanResults(); // clear results from BLEScan buffer to release memory
    vTaskDelay(cycleTime / portTICK_PERIOD_MS);
  }
}

void blink(int lowTime, int highTime){
  for(auto i=0; i<1400;){
    digitalWrite(LED_BUILTIN, LED_INVERTED);  
    delay(lowTime);                      
    digitalWrite(LED_BUILTIN, !LED_INVERTED);   
    delay(highTime); 
    i+= lowTime + highTime;                     
  }
}

void connectToWifiAndGetDST(){
  //stop and destroy sntp service//stop smooth time adjustment and set local time manually
  timeval epoch = {0, 0}; //Jan 1 1970
  settimeofday((const timeval*)&epoch, 0);

  Serial.print("Connecting to Wifi SSID ");
  Serial.print(wifiManager.getWiFiSSID());

  //turn LED on
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW); 

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the default name
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(AP_NAME, DEFAULT_AP_PW)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.restart();
    delay(5000);
  }

  Serial.print("\nWiFi connected. IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.BSSIDstr());
  Serial.print("Retrieving time: ");
  configTime(0, 3600, "time1.google.com"); // get UTC time via NTP
  time_t now = time(nullptr);
  while (now < 3 * 60)
  {
    Serial.print(".");
    delay(100);
    now = time(nullptr);
  }
  //if DST couldn't be aquired set manual time (which will be off, but maybe at least the year is close enough for a secure connection)
  if(now < 4 * 60){
    //stop smooth time adjustment and set local time manually
    timeval epoch = {1695159464, 0}; //Sep 19 2023
    settimeofday((const timeval*)&epoch, 0);
  }
  Serial.println(now);

  //update time stamps
  lastTimeoutCheck = time(nullptr);

  //turn LED off
  digitalWrite(LED_BUILTIN, HIGH);
}

void serialPrintNewReading(BParasite_Data_S *data, int i){
  Serial.println();
  Serial.printf("Sensor of %d: %s\n", i, data->name.c_str());
  Serial.printf("%.2f°C\n", data->temperature/100.0);
  Serial.printf("%.2f%% humidity\n", data->humidity/100.0);
  Serial.printf("%.3fV\n",  data->batt_voltage/1000.0);
  Serial.printf("%.2f%% soil moisture\n", data->soil_moisture/100.0);
  Serial.printf("%.2flux\n", data->illuminance/100.0);
  Serial.printf("%ddBm\n",  data->rssi);
  Serial.println();
}