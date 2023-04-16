#include "HAIntergas.h"
#include "WemosSerial.h"
#include <ESP8266WiFi.h>
#include <HAMqtt.h>
#include <LED.h>
#include <ArduinoOTA.h>
#include <Clock.h>
#include <Timer.h>
#include <DatedVersion.h>
DATED_VERSION(0, 9)
#include "secrets.h"

////////////////////////////////////////////////////////////////////////////////////////////
// Configuration
const char* sta_ssid      = STA_SSID;
const char* sta_pswd      = STA_PASS;

const char* mqtt_server   = "192.168.2.170";   // test.mosquitto.org"; //"broker.hivemq.com"; //6fee8b3a98cd45019cc100ef11bf505d.s2.eu.hivemq.cloud";
int         mqtt_port     = 1883;             // 8883;
const char* mqtt_user     = MQTT_USER;
const char *mqtt_passwd   = MQTT_PASS;

////////////////////////////////////////////////////////////////////////////////////////////
// Global instances
LED               led(D1);                    // pin D4 is used for TX so we can not use the onboard LED
WiFiClient        socket;                     // the client socket used to connect to mqtt
HAIntergas        ketel(D2);                  // THe intergas boiler HA device with all of its sensors
HAMqtt            mqtt(socket, ketel, INTERGAS_SENSOR_COUNT);  // Home Assistant MTTQ    we are at 14 sensors, so set to 20
WemosSerial       wemos_serial;               // the special serial used on a WEMOS version ESP8266
Clock             rtc;                        // A real (software) time clock

////////////////////////////////////////////////////////////////////////////////////////////
// For remote logging the log include needs to be after the global MQTT definition
#define LOG_REMOTE
#define LOG_LEVEL 2
#include <Logging.h>

void LOG_CALLBACK(char *msg) { 
  LOG_REMOVE_NEWLINE(msg);
  mqtt.publish("Intergas/log", msg, true); 
}

////////////////////////////////////////////////////////////////////////////////////////////
// Connect to the STA network
void wifi_connect() 
{ 
  if (((WiFi.getMode() & WIFI_STA) == WIFI_STA) && WiFi.isConnected())
    return;

  DEBUG("Wifi connecting to %s.", sta_ssid);
  WiFi.begin(sta_ssid, sta_pswd);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DEBUG(".");
  }
  DEBUG("\n");
  INFO("WiFi connected with IP address: %s\n", WiFi.localIP().toString().c_str());
}

///////////////////////////////////////////////////////////////////////////////////////
// Time sync
void sync_rtc() {
  rtc.ntp_sync();
  INFO("Clock synchronized to %s\n", rtc.now().timestamp().c_str());
}

////////////////////////////////////////////////////////////////////////////////////////////
// MQTT Connect
void mqtt_connect() {
  INFO("Intergas Logger v%s saying hello\n", VERSION);
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
bool retrieve_status(DateTime &now, const char*cmd) 
{
  INFO("[%s] - Sending message to boiler: %s\n", 
              now.timestamp(DateTime::TIMESTAMP_TIME).c_str(), 
              cmd);  
  wemos_serial.print(cmd);

  int i=0;
  do {
    delay(200);
    if (i++>3) {
      ERROR("No response\n");
      return false;
    }
  } while (!wemos_serial.available());

  byte buffer[64];
  for (i=0; i<sizeof(buffer) && wemos_serial.available(); i++) 
    buffer[i] = wemos_serial.read();

  INFO("Response from boiler of %d bytes\n", i);  
  DEBUG_BIN("Response from boiler: ", buffer, i);

  if (!ketel.status(buffer, i, cmd)) {
    ERROR("Error processing status\n");
    return false;
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
Timer interval;

enum Task {
  WAIT,
  STATUS1,
  STATUS2,
  STATUS3,
  SENSORS
};

int current_task = WAIT;

int scheduler(DateTime &now)
{
  if (!interval.passed())
    return WAIT;

  led.blink();

  // TODO: we may adjust the timers depending on the month (summertime) or on room temp
  // there is no use in updating all sensors when there is no heating demand
  // Add this smart logic to HAIntergas and have it return the interval timing
  
  interval.set(1000);               // interval between readings is 1sec to keep them in sync
  if (++current_task > SENSORS)       // set next task, rollover when reached the end
    current_task = STATUS1;

  bool active = (ketel.state != HAIntergas::IDLE);
  if (!active && (current_task == SENSORS))
    interval.set(10000);    // between readings the interval = 10s, except when there is demand

  return current_task;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
void setup() 
{
  wemos_serial.begin(9600);
  INFO("\n\nIntergas Logger Version %s\n", VERSION);
  wifi_connect();                      // 4) connect with WiFi
  sync_rtc();

  INFO("Connecting to MQTT server %s\n", mqtt_server);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  ketel.begin(mac, &mqtt);             // 5) make sure the device gets a unique ID (based on mac address)
  mqtt.onConnected(mqtt_connect);      // register function called when newly connected
  mqtt.begin(mqtt_server, mqtt_port, mqtt_user, mqtt_passwd);  // 

  if (!ketel.logmsg.isEmpty())
    ERROR(ketel.logmsg.c_str());

  INFO("Initialize OTA\n");
  ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname("Intergas-Logger");
  ArduinoOTA.setPassword(OTA_PASS);

  ArduinoOTA.onStart([]() {
    INFO("Starting remote software update");
  });
  ArduinoOTA.onEnd([]() {
    INFO("Remote software update finished");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
  });
  ArduinoOTA.onError([](ota_error_t error) {
    ERROR("Error remote software update");
  });
  ArduinoOTA.begin();
  INFO("Setup complete\n\n");
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
void loop() 
{
  // ensure we are still connected (STA-mode)
  wifi_connect();
  // handle any OTA requests
  ArduinoOTA.handle();
  // handle MQTT
  mqtt.loop();
  // whats the time
  DateTime now = rtc.now();
  // now lets deterime what we are going to do

  switch(scheduler(now)) {
    default:
    case WAIT:       
      break;
    case STATUS1:
      retrieve_status(now, HAIntergas::STATUS_1);      break;
    case STATUS2:
      retrieve_status(now, HAIntergas::STATUS_2);      break;
    case STATUS3:
      retrieve_status(now, HAIntergas::STATISTICS);    break;
    case SENSORS:
      INFO("[%s] - Reading temperature sensors\n", 
                now.timestamp(DateTime::TIMESTAMP_TIME).c_str());
      if (ketel.sensors())
        ERROR(ketel.logmsg.c_str());
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

