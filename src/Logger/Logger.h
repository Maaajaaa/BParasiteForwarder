#include <FS.h>
#include <LittleFS.h>
#include <time.h>

#include "BParasite.h"
#include "config.h"
#include <fstream>
#include "MaaajaaaClient/MaaajaaaClient.h"
/* You only need to format LittleFS the first time you run a
   test or else use the LITTLEFS plugin to create a partition
   https://github.com/lorol/arduino-esp32littlefs-plugin
   */

#define FORMAT_LITTLEFS_IF_FAILED true

#define LOG_TEMPERATURE
#define LOG_HUMIDITY
#define LOG_SIGNAL

#define FILE_WRITTEN_SUCCESSFULLY 200
#define FILE_EPERM 1 //operation not permitted
#define FILE_EISDIR 21 //Is a directory
#define FILE_ENOENT 2 //no such file or directory
#define FILE_ENOSPC 28 //No space left on device

class Logger{
    public:
        Logger();
        bool begin();
        bool cacheData(String MAC, BParasite_Data_S data, time_t time);
        static int logError(const char *logtext, va_list args);
        static int logErrorToFile(char *logtext);
        int checkForUnsyncedData(std::vector<std::string> knownBLEAdresses, bool syncWithServer);


    private:
        static int writeFile(const char * path, const char * message, const char * mode= FILE_WRITE);
        static int appendFile(const char * path, const char * message);
        void printFileSizeAndName(std::string filenamepath);

        bool successfulStart;
        std::vector<std::string> logFileNames;
        static String logHeaderString();
};