/*

*/

#include "HAIntergas.h"
#include <HAMqtt.h>
#include <String.h>
#include <DatedVersion.h>
DATED_VERSION(1, 0)

////////////////////////////////////////////////////////////////////////////////////////////
//#define LOG(s)    Serial.print(s)
#define LOG(s)    
#define GAS_FLOW  38.7    // gasflow in ml/sec (cm3/sec)
#define GAS_WATT  1361    // gasflow watt (1cm3 = 35.17 Joule)
#define GAS_USAGE_CALIBRATED  11527.78

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////
const char *HAIntergas::PROD_CODE = "B?\r";
const char *HAIntergas::STATUS_1  = "S?\r";
const char *HAIntergas::STATUS_2  = "S2\r";
const char *HAIntergas::PARAMS    = "V?\r";
const char *HAIntergas::SETTINGS  = "V1\r";
const char *HAIntergas::STATISTICS= "HN\r"; 

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
bool HATempSensor::begin(DallasTemperature *interface, int idx) {
  return interface->getAddress(address, idx);
}

bool HATempSensor::loop(DallasTemperature *interface) {
  float temp = interface->getTempC(address);
  if (temp < -5 || temp > 100)
    return false;
 
  return setValue(temp);
}

////////////////////////////////////////////////////////////////////////////////////////////
#define CONSTRUCT_P0(var)       var(#var, HABaseDeviceType::PrecisionP0)
#define CONSTRUCT_P2(var)       var(#var, HABaseDeviceType::PrecisionP2)

#define CONFIGURE_BASE(var, name, class, icon)  var.setName(name); var.setDeviceClass(class); var.setIcon("mdi:" icon)
#define CONFIGURE(var, name, class, icon, unit) CONFIGURE_BASE(var, name, class, icon); var.setUnitOfMeasurement(unit)
#define CONFIGURE_TEMP(var, name, icon)         CONFIGURE(var, name, "temperature", icon, "°C")
#define CONFIGURE_DS(var, icon)                 CONFIGURE_TEMP(var, #var, icon)

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
HAIntergas::HAIntergas(int wire_pin)
: mode("mode"), alarm("alarm"), CONSTRUCT_P0(fault_code), CONSTRUCT_P0(last_fault),
  CONSTRUCT_P2(T_boiler),     CONSTRUCT_P2(T_boiler_in),    CONSTRUCT_P2(T_boiler_out),   CONSTRUCT_P2(T_ww_out),  CONSTRUCT_P2(T_set),        
  CONSTRUCT_P2(pressure),     CONSTRUCT_P0(fan_set),    CONSTRUCT_P0(fan_cur),    CONSTRUCT_P2(fan_pwm),   CONSTRUCT_P2(pump_pwm),  CONSTRUCT_P2(tap_flow),
  CONSTRUCT_P2(T_room_set),   CONSTRUCT_P2(T_room_cur), 
  CONSTRUCT_P2(power),        CONSTRUCT_P2(energy_cv),   CONSTRUCT_P2(energy_hw),   
  // DS sensors
  CONSTRUCT_P2(water_in), CONSTRUCT_P2(water_out), CONSTRUCT_P2(air_in), CONSTRUCT_P2(air_out), 
  CONSTRUCT_P2(mixed), CONSTRUCT_P2(exhaust), CONSTRUCT_P2(cv_out), CONSTRUCT_P2(cv_in),
  _wire(wire_pin), _sensors(&_wire)
{
  //generic
  CONFIGURE_BASE(mode,       "mode",       "enum",     "state-machine"); 
  CONFIGURE_BASE(alarm,      "alarm",      "enum",     "alarm-light"); 
  CONFIGURE_BASE(fault_code, "fault",      "enum",     "code-tags"); 
  CONFIGURE_BASE(last_fault, "last_code",  "enum",     "code-tags"); 
  // base temps
  CONFIGURE_TEMP(T_boiler,    "heater",     "gas-burner");
  CONFIGURE_TEMP(T_boiler_in, "heater-in",  "gas-burner");
  CONFIGURE_TEMP(T_boiler_out,"heater-out", "gas-burner");
  CONFIGURE_TEMP(T_ww_out,    "WW-out",     "thermometer-water");
  CONFIGURE_TEMP(T_set,       "max",        "thermostat-box");
  //operations
  CONFIGURE(    pressure,     "pressure",   "pressure",       "gauge",      "bar"); 
  CONFIGURE(    fan_cur,      "fan-rpm",    "speed",          "fan",        "rpm"); 
  CONFIGURE(    fan_set,      "fan_set",    "speed",          "fan",        "rpm"); 
  CONFIGURE(    fan_pwm,      "fan-pwm",    "power_factor",   "fan",        "%"); 
  CONFIGURE(    pump_pwm,     "pump",       "power_factor",   "shower",     "%"); 
  CONFIGURE(    tap_flow,     "tapflow",    "water",          "shower",     "l/m"); 
  // energy usage
  CONFIGURE(    power,        "power",      "power",          "meter-gas",  "kW"); 
  CONFIGURE(    energy_cv,    "energy_cv",  "gas",            "meter-gas",  "m³"); 
  CONFIGURE(    energy_hw,    "energy_hw",  "gas",            "meter-gas",  "m³");
  energy_cv.setStateClass("total_increasing");
  energy_hw.setStateClass("total_increasing");

  // thermostat
  CONFIGURE_TEMP(T_room_set,  "room_set", "thermostat");
  CONFIGURE_TEMP(T_room_cur,  "room_cur", "home-thermometer");

  // DS sensors
  CONFIGURE_DS(water_in,  "thermometer-low");
  CONFIGURE_DS(water_out, "thermometer-high");
  CONFIGURE_DS(mixed,     "water-thermometer");
  CONFIGURE_DS(air_in,    "home-thermometer-outline");
  CONFIGURE_DS(air_out,   "snowflake-thermometer");
  CONFIGURE_DS(exhaust,   "thermometer");
  CONFIGURE_TEMP(cv_out,  "CV-out", "water-thermometer-outline");
  CONFIGURE_TEMP(cv_in,   "CV-in","water-thermometer");
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
bool HAIntergas::begin(const byte mac[6], HAMqtt *mqtt) 
{
  logmsg.clear();
  setUniqueId(mac, 6);
  setManufacturer("InnoVeer");
  setName("Intergas HRE24/18");
  setSoftwareVersion(VERSION);
  setModel("Intergas Logger esp8266");

  // generic heater
  mqtt->addDeviceType(&mode);     // register the sensors
  mqtt->addDeviceType(&fault_code);
  mqtt->addDeviceType(&last_fault);
  mqtt->addDeviceType(&alarm);
  // base temperatures
  mqtt->addDeviceType(&T_boiler); 
  mqtt->addDeviceType(&T_boiler_out); 
  mqtt->addDeviceType(&T_boiler_in); 
  mqtt->addDeviceType(&T_ww_out); 
  mqtt->addDeviceType(&T_set); 
  // speed and pressure
  mqtt->addDeviceType(&pressure);
  mqtt->addDeviceType(&fan_set);
  mqtt->addDeviceType(&fan_cur); 
  mqtt->addDeviceType(&fan_pwm); 
  mqtt->addDeviceType(&pump_pwm); 
  mqtt->addDeviceType(&tap_flow); 
  // usage
  mqtt->addDeviceType(&power);
  mqtt->addDeviceType(&energy_cv);
  mqtt->addDeviceType(&energy_hw);
  // thermostate
  mqtt->addDeviceType(&T_room_set); 
  mqtt->addDeviceType(&T_room_cur); 

  // boiler
  mqtt->addDeviceType(&water_in);  
  mqtt->addDeviceType(&water_out);  
  mqtt->addDeviceType(&air_in);  
  mqtt->addDeviceType(&air_out);  
  mqtt->addDeviceType(&mixed);  
  // environment
  mqtt->addDeviceType(&exhaust);  
  mqtt->addDeviceType(&cv_out);  
  mqtt->addDeviceType(&cv_in);  

  _sensors.begin();
  if (_sensors.getDeviceCount() < 8)
    logmsg = "ERROR: We have not found the 8 DS sensors";

  bool result = true;
  result &= water_in.begin(&_sensors, 0);
  result &= water_out.begin(&_sensors, 1);
  result &= air_in.begin(&_sensors, 2);
  result &= air_out.begin(&_sensors, 3);
  result &= mixed.begin(&_sensors, 4);
  result &= exhaust.begin(&_sensors, 5);
  result &= cv_out.begin(&_sensors, 6);
  result &= cv_in.begin(&_sensors, 7);

  if (!result)
    logmsg = "ERROR: One of the sensors did not give its address";

  return result;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
bool HAIntergas::status(const byte *buffer, int lg, const char *instruction)
{
  logmsg.clear();
  if (instruction == STATUS_1)
    return _status_1(buffer, lg);
  if (instruction == STATUS_2)
    return _status_2(buffer, lg);
  if (instruction == STATISTICS)
    return _statistics(buffer, lg);

  return false;
}

bool HAIntergas::sensors()
{
  logmsg.clear();
  _sensors.requestTemperatures();   // send command to sensors to measure

  bool result = true;
  result &= water_in.loop(&_sensors);       // retrieve temp
  result &= water_out.loop(&_sensors);  
  result &= air_in.loop(&_sensors);  
  result &= air_out.loop(&_sensors);  
  result &= mixed.loop(&_sensors);  
  result &= exhaust.loop(&_sensors);  
  result &= cv_out.loop(&_sensors);  
  result &= cv_in.loop(&_sensors);  

  if (!result)
    logmsg = "ERROR: getting/setting one of the temperatures";
  return result;
}

////////////////////////////////////////////////////////////////////////////////////////////
// value Setters with limits for validation
bool HAIntergasSensor::set(float value, float min, float max) 
{
  if (value >= min && value <= max)
    return setValue(value); // value is within expected limits so post the value

  setCurrentValue(0.0f);    // set the current value to force a post on the first valid value 
  return false;
}

bool HAIntergasSensor::set(uint16_t value, uint16_t min, uint16_t max)
{
  if (value >= min && value <= max)
    return setValue(value); // value is within expected limits so post the value

  setCurrentValue(0);    // set the current value to force a post on the first valid value 
  return false;
}

////////////////////////////////////////////////////////////////////////////////////////////
// helpers
float getfloat(uint8_t lsb, uint8_t msb) {
  if (msb > 127)
    return -(float((msb ^ 255) + 1) * 256 - lsb) / 100.0;
  //else
  return float(msb * 256 + lsb) / 100.0;
}

////////////////////////////////////////////////////////////////////////////////////////////
// IDS-log: t1,t2,t3,t4,t5,t6,temp_set,fan_set,fanspeed,fan_pwm,opentherm,roomtherm,tap_switch,gp_switch,pump,dwk,gasvalve,io_signal,spark,io_curr,displ_code,ch_pressure
// XTREME:  https://github.com/little-chef/intergas-xtreme-monitor/blob/main/esphome/IntergasXtremeMonitor.h
// HRE:     https://github.com/RichieB2B/intergas-exporter/blob/main/intergas-exporter.py
////////////////////////////////////////////////////////////////////////////////////////////
bool HAIntergas::_status_1(const byte *sbuf, int lg)
{
  if (lg<25) {
    logmsg = "ERROR: processing S? result -> too short";
    return false;
  }
  LOG("Processing state 1 result\n");
  bool result = true;

  result &= T_boiler.set(getfloat(sbuf[ 0], sbuf[ 1]), 10.0f, 100.0f);
  result &= T_boiler_out.set(getfloat(sbuf[ 2], sbuf[ 3]), 20.0f,  70.0f);
  result &= T_boiler_in.set (getfloat(sbuf[ 4], sbuf[ 5]), 15.0f,  70.0f);
  result &= T_ww_out.set(getfloat(sbuf[ 6], sbuf[ 7]), 20.0f,  70.0f);
//result &= T_ww_in.set( getfloat(sbuf[ 8], sbuf[ 9]);                // NC, always -50.81
//result &= T_outside.set(getfloat(sbuf[10], sbuf[11]);                // NC, always -50.81
  result &= pressure.set(getfloat(sbuf[12], sbuf[13]),  0.0f,  5.0f);
  result &= T_set.set(   getfloat(sbuf[14], sbuf[15]), 20.0f, 70.0f);
  uint16_t fan= getfloat(sbuf[16], sbuf[17]) * 100;                   // target fanspeed, remember the fanspeed for boiler modus
  result &= fan_set.set( fan,                              0,  7000); // max speed is 6500rpm for a HRE 36/48, 4600 for 28/24 
  result &= fan_cur.set( getfloat(sbuf[18], sbuf[19])*100, 0,  7000); // current fanspeed
  result &= fan_pwm.set( getfloat(sbuf[20], sbuf[21]) *10, 0,  100);     // with a minimu setting of 20% we read 17.5 
  result &= power.set(   getfloat(sbuf[22], sbuf[23]) *GAS_WATT/1000, 0, 30); // using the io_current to estimate the gas usage in Watts

  _bstate = sbuf[24];

/*  data['gp_switch']          = bool(ig_raw[26] &  1 << 7)
    data['tap_switch']         = bool(ig_raw[26] &  1 << 6)
    data['roomtherm']          = bool(ig_raw[26] &  1 << 5)
    data['pump']               = bool(ig_raw[26] &  1 << 4)
    data['dwk']                = bool(ig_raw[26] &  1 << 3)
    data['alarm_status']       = bool(ig_raw[26] &  1 << 2)
    data['ch_cascade_relay']   = bool(ig_raw[26] &  1 << 1)
    data['opentherm']          = bool(ig_raw[26] &  1 << 0)
    data['gasvalve']           = bool(ig_raw[28] &  1 << 7)
    data['spark']              = bool(ig_raw[28] &  1 << 6)
    data['io_signal']          = bool(ig_raw[28] &  1 << 5)
    data['ch_ot_disabled']     = bool(ig_raw[28] &  1 << 4)
    data['low_water_pressure'] = bool(ig_raw[28] &  1 << 3)
    data['pressure_sensor']    = bool(ig_raw[28] &  1 << 2)
    data['burner_block']       = bool(ig_raw[28] &  1 << 1)
    data['grad_flag']          = bool(ig_raw[28] &  1 << 0)
*/
  result &= alarm.setState(sbuf[26] & (1 << 2));

  if (sbuf[27] == 128)          // and what about the alarm bit?
    result &= fault_code.setValue(sbuf[29]);      // current listed fault code is the active fault code
  else
  {
    result &= last_fault.setValue(sbuf[29]);
    result &= fault_code.setValue(0);
  }
  bool lock = bool(sbuf[28] & (1 << 1));
  if (lock) {
      result = mode.setValue("lock");
      state = LOCK;
  }
  else
  {
    switch (_bstate) 
    {
    case 126:                     // 0x7E
      result = mode.setValue("idle");
      state = IDLE; break;            // tempature reached, boiler in idle mode
    case 231:                     // 0xE7 
      result = mode.setValue("spindown");
      state = SPINDOWN; break;             // spindown (nadraaien) after a burn cycle
    case 0: 
      if (fan > 0) {    // heat is requested (Tmax>0), but are we also heating?
        result = mode.setValue("heating");
        state = HEATING;         // yes we are heating
      } else {
        result = mode.setValue("standby");
        state = STANDBY;         // no, we are waiting for CV water temp to drop
      }
      break;
  //  case 102:   // CV burns -> false, this is 0. 
  //  case 51:    // Hot water-> false, maybe its spondown from HW
    case 204: 
      result = mode.setValue("hot_water");
      state = HOT_WATER;   break;
    default:  
      result = mode.setValue(("code 0x" + String(_bstate, 16)).c_str());
      state = UNKNOWN;    break;
    }
  }
  if (!result)
    logmsg = "ERROR: processing return S? command";

  return result;  // return the result of posting the boiler mode
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
bool HAIntergas::_status_2(const byte *sbuf, int lg)
{
  if (lg<20) {
    logmsg = "ERROR: processing return S2 command. Result too short";
    return false;
  }
  LOG("Processing state 2 result\n");

  bool result = true;

  result &= tap_flow.set(getfloat(sbuf[0], sbuf[1]), 0.0f, 20.0f);
  result &= pump_pwm.set((200 - sbuf[2])/2.0, 0.0f, 100.0f); // expecting max 100%
  //  T_z1_override = getFloat(sbuf[5], sbuf[4]);
  result &= T_room_set.set(getfloat(sbuf[6], sbuf[7]), 10.0f, 30.0f);     // requested room temperature zone 1
  result &= T_room_cur.set(getfloat(sbuf[8], sbuf[9]), 10.0f, 40.0f);     // current room temperature zone 1
  //  T_z2_override = getFloat(sbuf[11], sbuf[10]);   // ?
  //  T_z2_set      = getFloat(sbuf[13], sbuf[12]);   // requested room temperature zone 2
  //  T_z2_cur      = getFloat(sbuf[15], sbuf[14]);   // current room temperature
  //  outside   = getFloat(sbuf[17], sbuf[16]);   // override_outside_temp?     always 327.67 (0xFFFF = signed -1)

  // data['OT_master_member_id']   = ig_raw[3]
  // data['OT_therm_prod_version'] = ig_raw[18]
  // data['OT_therm_prod_type']    = ig_raw[19]  
  
  if (!result)
    logmsg = "ERROR: processing return S? command";
  return result;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////////////////////////
float gas_used_calibrated(float value, bool cv) {

//  return value / GAS_USAGE_CALIBRATED;
  float calibrated;
  if (cv)                                                        // calibrated per march 6, 2023
    calibrated = (value - 54028399.0f) / 11619.27f + 4686.8f;    // take the statistics of today, apply a (new) factor and add the offset communicated the day before
  else // hw
    calibrated = (value - 806253.0f)   / 11619.27f + 69.94f;

  return calibrated;
}

bool HAIntergas::_statistics(const byte *sbuf, int lg)
{
  if (lg<24) {
    logmsg = "ERROR: processing statistics result -> too short";
    return false;
  }
  LOG("Processing statistics result\n");

  bool result = true;

  result &= energy_cv.set(gas_used_calibrated(float(sbuf[16] + (sbuf[17] << 8) + (sbuf[18] << 16) + (sbuf[19] << 24)), true), 0.0f, 15000.0f);   // heating is currently at 5375 m3
  result &= energy_hw.set(gas_used_calibrated(float(sbuf[20] + (sbuf[21] << 8) + (sbuf[22] << 16) + (sbuf[23] << 24)), false), 0.0f,  1000.0f);   // water is currently at 70 m3
//  result &= water_total.set(float(sbuf[24] + (sbuf[25] << 8) + (sbuf[28] << 16)) / 10000.0f, 0.0f, 50000.0f);

  /*data['line_power_connected']  = Get_int(ig_raw[0], ig_raw[1]) + ig_raw[30] * 65536
      data['line_power_disconnect'] = Get_int(ig_raw[2], ig_raw[3])
      data['ch_function']           = Get_int(ig_raw[4], ig_raw[5])
      data['dhw_function']          = Get_int(ig_raw[6], ig_raw[7])
      data['burnerstarts']          = Get_int(ig_raw[8], ig_raw[9]) + ig_raw[31] * 65536
      data['ignition_failed']       = Get_int(ig_raw[10], ig_raw[11])
      data['flame_lost']            = Get_int(ig_raw[12], ig_raw[13])
      data['reset']                 = Get_int(ig_raw[14], ig_raw[15])
      data['gasmeter_cv']           = Get_int32(ig_raw[16:20]) / float(10000)
      data['gasmeter_dhw']          = Get_int32(ig_raw[20:24]) / float(10000)
      data['watermeter']            = Get_int32(ig_raw[24:26] + ig_raw[28].to_bytes(2,'little')) / float(10000)
      data['burnerstarts_dhw']      = Get_int32(ig_raw[26:28] + ig_raw[29].to_bytes(2,'little'))
*/
  return result;
}

////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

