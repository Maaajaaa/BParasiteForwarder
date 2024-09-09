
#define TAG "majaStuff"
#define myTAG "majaStuff"
#define ERROR_LOG_FILE "/errors.log"
#define CACHE_FILE "/cache.log"
#define PATH_2 "/old"
#define WIFI_CONF_FILE "/wifi.json"

#define SECOND_conf 1

#define LED_INVERTED true

#define CONFIG_ARDUHAL_LOG_DEFAULT_LEVEL_ERROR 1
#define CONFIG_ARDUHAL_LOG_DEFAULT_LEVEL 1
#define CONFIG_ARDUHAL_ESP_LOG 1

#define TIME_BEFORE_RECONNECT 120
#define WIFI_CONNECTION_WAIT 5*60

#define CACHE_FILE_LIMIT_BYTES 250 * 1000
#define ERROR_LOG_FILE_LIMIT 250 * 1000

//level beyond which the warning message is sent, in the future this should be fetched per-sensor from the server
#define LOW_MOISTURE_LEVEL 35.0

//critical warnings are sent continuously
#define CRITICAL_WARNING_LEVEL 15.0

//time that the sensor has to be offline for a warning to appear in minutes, remember that b-parasite only updates every 10 minutes per default
#define OFFLINE_WARNING_TIME 55

//send a thank you if watering is back up above this level after being low
#define WATERING_THANKYOU_LEVEL 50


//Serial Debugging in Messenger Class
#define MESSENGER_SERIAL_DEBUG 1

//ESP Core debug
//#ifdef CORE_DEBUG_LEVEL
//#undef CORE_DEBUG_LEVEL
//#endif

//#define CORE_DEBUG_LEVEL 3
//#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG