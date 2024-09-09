#include "MaaajaaaClient.h"

static const char *lTag = "maaajaaaClient";


MaaajaaaClient::MaaajaaaClient()
{
    secured_client.setCACert(howmysslkey);
}

int MaaajaaaClient::begin(){
    if(!began){
        began = true;
        return this->connectToServer();
    }else{
        return -2;
    }
}

void MaaajaaaClient::end(){
    secured_client.stop();
    this->began = false;
}

int MaaajaaaClient::connectToServer(){
    /*return*/
    ESP_LOGI(lTag, "connecting to DB-server %s" dbServer);
    //secured_client.connect(dbServer, 443);
    if (!secured_client.connect(dbServer, 443)){
        char errorBuff[255];
        secured_client.lastError(errorBuff, 255);
        ESP_LOGE(lTag, "Connection to maaajaaa.de FAILED error: %s", errorBuff);
        return -1;
    }
    else
    {
        ESP_LOGI(lTag, "connected to maaajaaa.de");
        return 1;
    }
}

int MaaajaaaClient::sendRequestGetRespCode(String reqUrl, String data){
    long responseCode = -1;
    /*return*/
    if(!secured_client.connected()){
        Serial.println("connecting to Maaajaaa for request");
         if (!secured_client.connect(dbServer, 443)){
            Serial.println("Connection attempt failed!");
            return -1;
         }
    }
    if(!secured_client.connected()){
        Serial.println("Connection failed!");
    }
    else
    {
        Serial.println("Connected to maaajaaa.de for request");
        secured_client.print("POST https://");
        secured_client.print(reqUrl);
        secured_client.println(" HTTP/1.0");
        secured_client.print("Host: ");
        secured_client.println(dbServer);
        secured_client.println("Content-Type: application/x-www-form-urlencoded");
        secured_client.print(F("Content-Length: "));
        secured_client.println(data.length());
        secured_client.println();
        secured_client.println(data);
        secured_client.println(F("Connection: close"));
        if(secured_client.println() == 0){
            log_e("failed to send request %s", reqUrl);
            return -1;
        }

        String responseHeader;
        if(secured_client.connected() && responseHeader.length() <= MAX_HEADER_LENGTH){
            responseHeader = secured_client.readStringUntil('\n');
            Serial.print("got response: ");
            Serial.println(responseHeader);
            int firstSpace = responseHeader.indexOf(' ');
            Serial.println(firstSpace);
            Serial.println(responseHeader.substring(firstSpace+1,firstSpace+4));


            responseCode = responseHeader.substring(firstSpace+1,firstSpace+4).toInt();
            if(responseCode != 0){
                if(responseCode %100 != 2){
                    ESP_LOGD(lTag, "got response %i for %s", reqUrl);
                }
                ESP_LOGI(lTag, "responseCode: %i", responseCode);
            }else{
                ESP_LOGE(lTag, "failed to read response code in %s", responseHeader);
            }
        }

        // if there are incoming bytes available
        // from the server, read them and print them:
        while (secured_client.available()) {
            char c = secured_client.read();
            Serial.write(c);
        }
    }

    return responseCode;
}

int MaaajaaaClient::connected(){
    return secured_client.connected();
}

/*int MaaajaaaClient::createSensor(String MAC, String plantName){
    String url = dbServer;
    url += "/createSensor.php";
    String data = "name=" + plantName;
    data += "&MAC=" + MAC;
    Serial.print("CREATESENSOR ");
    Serial.println(url);
    return sendRequestGetRespCode(url, data);
}*/

int MaaajaaaClient::logReading(BParasite_Data_S data, String MAC, String readingTime){
    String url = dbServer;
    url += "/logData.php";
    String content = "MAC=" + MAC;
    content += "&soilMoisture=" + String((float) data.soil_moisture/100);
    content += "&temperature=" + String((float) data.temperature/100);
    content += "&humidity=" + String((float) data.humidity/100);
    content += "&batteryVoltage=" + String((float) data.batt_voltage/1000);
    content += "&readingTime=" + readingTime;
    content += "&signalStrength=" + String(data.rssi);
    Serial.printf("sending log %s", content.c_str());
    return sendRequestGetRespCode(url, content);
}