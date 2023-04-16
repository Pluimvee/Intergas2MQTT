#ifndef HOME_ASSIST_INTERGAS
#define HOME_ASSIST_INTERGAS

#include <HADevice.h>
#include <device-types\HASensorNumber.h>
#include <device-types\HABinarySensor.h>
#include <utils\HASerializer.h>
#include <DallasTemperature.h>
#include <OneWire.h>

#define INTERGAS_SENSOR_COUNT 30    // its actually 28 but well..... give it some slack

////////////////////////////////////////////////////////////////////////////////////////////
// Intergas sensors
class HAIntergasSensor : public HASensorNumber
{
public:
  HAIntergasSensor(const char*id, const NumberPrecision p) : HASensorNumber(id, p) {};
  bool      set(float value, float min, float max); // sets a float when value is in between min, max
  bool      set(uint16_t value, uint16_t min, uint16_t max); // sets a word when value is in between min, max
};

////////////////////////////////////////////////////////////////////////////////////////////
//
class HATempSensor : public HASensorNumber
{
private:
  byte address[8];
public:
  HATempSensor(const char*id, const NumberPrecision p) : HASensorNumber(id, p) {};
  bool begin(DallasTemperature *interface, int idx);
  bool loop(DallasTemperature *interface);
};


////////////////////////////////////////////////////////////////////////////////////////////
class HAIntergas : public HADevice 
{
private:
  OneWire            _wire;
  DallasTemperature  _sensors;
  uint8_t            _bstate;

  bool _status_1(const byte *buffer, int lg);
  bool _status_2(const byte *buffer, int lg);
  bool _statistics(const byte *buffer, int lg);
public:
  static const char *PROD_CODE;
  static const char *STATUS_1;
  static const char *STATUS_2;
  static const char *PARAMS;
  static const char *SETTINGS;
  static const char *STATISTICS;

  enum mode {
    UNKNOWN,    // unknown status
    IDLE,       // no heating requested
    STANDBY,    // heating requested, awaiting for CV-in to lower
    SPINDOWN,
    LOCK,       // cycle after burn cycle to prevent 
    HEATING,    // central heating
    HOT_WATER,  // hot water
  } state;

  HAIntergas(int wire_pin);

  // generic heater
  HASensor          mode;      // will always post to mqtt, also serves as 'alive' message
  HABinarySensor    alarm;      
  HAIntergasSensor  fault_code;
  HAIntergasSensor  last_fault;
  // base temperatures
  HAIntergasSensor  T_boiler;
  HAIntergasSensor  T_boiler_out;
  HAIntergasSensor  T_boiler_in;
  HAIntergasSensor  T_ww_out;
  HAIntergasSensor  T_set;     
  // operations
  HAIntergasSensor  pressure;
  HAIntergasSensor  fan_set;   // fan set in rpm
  HAIntergasSensor  fan_cur;   // fan current in rpm
  HAIntergasSensor  fan_pwm;   // fan PWM duty cycle percentage
  HAIntergasSensor  pump_pwm;  // pump PWM duty cycle percentage
  HAIntergasSensor  tap_flow;
  // thermostate
  HAIntergasSensor  T_room_set;
  HAIntergasSensor  T_room_cur;
  // energy usage
  HAIntergasSensor  power;      // using ionization current (io_curr = flame detection) in uA to calculate the power in kW
  HAIntergasSensor  energy_cv;  // total gas used for heating
  HAIntergasSensor  energy_hw;  // total gas used for hot water

  // boiler
  HATempSensor  water_in;
  HATempSensor  water_out;
  HATempSensor  air_in;
  HATempSensor  air_out;
  HATempSensor  mixed;

  // environment
  HATempSensor  exhaust;
  HATempSensor  cv_out;
  HATempSensor  cv_in;

  bool begin(const byte mac[6], HAMqtt *mqqt);
  bool status(const byte *buffer, int lg, const char *instruction); // parse the intergas serial response
  bool sensors();                                                   // read DS1820 sensors

  String logmsg; 
};

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

#endif