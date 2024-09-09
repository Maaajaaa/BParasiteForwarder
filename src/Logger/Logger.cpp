#include "Logger.h"

const char* lTag = "b-Logger";

Logger::Logger(){
}

/// @brief initialize the logging system, and log files as well as the file system
/// @return success of creating at least the log file
bool Logger::begin(){
    //set log levels
    esp_log_level_set("*", ESP_LOG_WARN);      // enable WARN logs from WiFi stack
    esp_log_level_set("wifi", ESP_LOG_DEBUG);      // enable WARN logs from WiFi stack
    esp_log_level_set("dhcpc", ESP_LOG_DEBUG);     // enable WARN logs from DHCP client
    esp_log_level_set("majaStuff", ESP_LOG_DEBUG);     // enable debug logs from majaStuff
    esp_log_level_set("b-Messenger", ESP_LOG_INFO);     // enable debug logs from b-Messenger
    esp_log_level_set("b-Logger", ESP_LOG_INFO);     // enable debug logs from b-Messenger
    esp_log_level_set("maaajaaaClient", ESP_LOG_INFO);     // enable debug logs from MaaajaaaClient

    //check for file system failure (which we can't write to a file if it happens)
    if(!LittleFS.begin(FORMAT_LITTLEFS_IF_FAILED)){
        Serial.println("LittleFS Mount Failed");
        return false;
    }
    
    //check for cache file
    if(!LittleFS.exists(CACHE_FILE)){
        writeFile(CACHE_FILE, logHeaderString().c_str());
        printFileSizeAndName(CACHE_FILE);
    }else{
        printFileSizeAndName(CACHE_FILE);
    }

    //check for error file
    if(!LittleFS.exists(ERROR_LOG_FILE)){
        //create empty log file
        if(writeFile(ERROR_LOG_FILE, "Bparasite ESP32 log file\n") == 1){
            printFileSizeAndName(ERROR_LOG_FILE);
            return true;
        }else{
            return false;
        }
    }else{
        return true;
    }
}

/// @brief Print filenamepath's filesize and name via Serial
/// @param filenamepath path of the file to be analyzed
void Logger::printFileSizeAndName(std::string filenamepath){
    File file = LittleFS.open(filenamepath.c_str(), FILE_READ);
    Serial.print(filenamepath.c_str());
    Serial.print(" exists, size: ");
    Serial.println(file.size());
    file.close();
}

/// @brief Take data and save it to the cache file, to be synced later
/// @param MAC mac-address of the sensor
/// @param data Baparasite data
/// @param time time of the reading
/// @return 1 if file is written successfully, else unix file error codes are used, see Logger.h
bool Logger::cacheData(String MAC, BParasite_Data_S data, time_t time){
    //log time
    String timeString = String(time);
    timeString += ";";
    //log MAC
    timeString += MAC;
    timeString += ";";
    //log soil moisture
    timeString += data.soil_moisture;
    #ifdef LOG_TEMPERATURE
    timeString += ";";
    timeString += data.temperature;
    #endif
    #ifdef LOG_HUMIDITY
    timeString += ";";
    timeString += data.humidity;
    #endif
    #ifdef LOG_SIGNAL
    timeString += ";";
    timeString += data.rssi;
    #endif  
    timeString += "\n";
    return appendFile(CACHE_FILE,timeString.c_str());
}


/// @brief Method for processing all kinds of esp-errors via the ESP-error system, and forwarding them
/// @param logtext message of the log
/// @param args arguments
/// @return 
int Logger::logError(const char *logtext, va_list args)
{
    char* string;
    int retVal = vasprintf(&string, logtext, args);
    int writeResult = logErrorToFile(string);
    Serial.print(string);
    //still write to stdout
    return vprintf(logtext, args);
}

/// @brief save the error string into the error file
/// @param logtext string to be saved
/// @return 1 if successful, unix file error code else
int Logger::logErrorToFile(char *logtext)
{
    std::string logLine =  std::string("[") 
        + std::to_string(time(nullptr)) + std::string("] ") 
        + std::string(logtext) + std::string("\n");
    
    Serial.print("Log text: ");
    Serial.println(logLine.c_str());
    return appendFile(ERROR_LOG_FILE, logLine.c_str());
}

/// @brief write a file, and create an archived version if the file is a log or cache file and exceeds the limits set in config.h
/// @param filepath path of the file
/// @param data data to write into the file
/// @param mode write mode
/// @return 1 if successful, unix file error code else
int Logger::writeFile(const char *filepath, const char *data, const char *mode)
{
    if( LittleFS.totalBytes() - LittleFS.usedBytes() <= sizeof(data)+2) return ENOSPC;
    //if(!SPIFFS.exists(filepath)) return ENOENT;
    File file = LittleFS.open(filepath, mode);
    //check if we need to start a new file
    if( (filepath == ERROR_LOG_FILE && file.available() &&  file.size() > ERROR_LOG_FILE_LIMIT) ||
        (filepath == CACHE_FILE && file.size() > CACHE_FILE_LIMIT_BYTES) ){
        //close and move the file
        file.close();
        String newFilePath = String(PATH_2) + String(filepath);
        if(LittleFS.exists(newFilePath)){
            LittleFS.remove(newFilePath.c_str());
        }
        bool success = LittleFS.rename(filepath, newFilePath.c_str());
        //create header if it's a data log file
        if(filepath == CACHE_FILE){
            writeFile(filepath, logHeaderString().c_str());
        }
        file = LittleFS.open(filepath, mode);

        //do this after opening the file so if there's no possibility for an infite loop if we (failed to) move the log file
        if(success){
            ESP_LOGD(lTag, "moved file %s to %s", filepath, newFilePath.c_str());
        }else{
            ESP_LOGE(lTag, "FAILED TO move file %s to %s", filepath, newFilePath.c_str());
        }
    }
    if(file.isDirectory())  return EISDIR;
    if(!file) return ENOENT;
    if(file.println(data)){
        Serial.print(filepath);
        Serial.println(" written/appended");
    } else {
        file.close();
        return EIO;
    }
    file.close();
    return 1;
}

/// @brief append to a file and use the writeFile fea
/// @param filepath path of the file
/// @param data data to write into the file
/// @return 1 if successful, unix file error code else
int Logger::appendFile(const char * filepath, const char * data){
    return writeFile(filepath, data, FILE_APPEND);
}

/// @brief generate the header for the logging csv to make it human-readable/hackable
/// @return string
String Logger::logHeaderString(){
    //create first line
    String logHeaderString = "unix timestamp in seconds (this formula =R2/86400+DATE(1970;1;1) , where R2 is the cell containing the timestamp);MAC-Address of the sensor ;moisture (divide by 100 to get %)";
    #ifdef LOG_TEMPERATURE
    logHeaderString += ";Temperature (divide by 100 for degrees celcius)";
    #endif
    #ifdef LOG_HUMIDITY
    logHeaderString += ";Humdity (divide by 100 for %)";
    #endif
    #ifdef LOG_SIGNAL
    logHeaderString += ";signal strenth in dBm";
    #endif
    logHeaderString += "\n";
    return logHeaderString;
}

int Logger::checkForUnsyncedData(std::vector<std::string> knownBLEAdresses, bool syncWithServer = false){
    return 0;
}