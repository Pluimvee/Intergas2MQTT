#include "WemosSerial.h"
#include <arduino.h>

////////////////////////////////////////////////////////////////////////
#define TXD2 D8   // GPIO15   // D8 -> may not be high at boot, so we can not connect this one to the boiler
#define RXD2 D7   // GPIO13   // D7 ->
#define TXD1 D4   // GPIO2    // D4 -> we need to use this one, although connected to the onboard LED
#define TXD0 TX   // GPIO1    // TX -> connected to CH430
#define RXD0 RX   // GPIO3    // RX -> connected to CH430

////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////
WemosSerial::WemosSerial()
: HardwareSerial(UART1)
{
}

bool WemosSerial::begin(int baudrate)
{
  Serial.begin(baudrate, SERIAL_8N1, SERIAL_RX_ONLY);                 // UART0 for rx only
  Serial.pins(TXD2, RXD2);                                            // using pin RXD2 (D7)
  HardwareSerial::begin(baudrate, SERIAL_8N1, SERIAL_TX_ONLY, TXD1);  // UART1 for TX, on D4
  return true;
}

bool WemosSerial::print(const char*s) {
  return HardwareSerial::print(s);
}

// serial input remains using UART0 on pin RX (RXD0)
int WemosSerial::available() {
  return Serial.available();
}

int WemosSerial::read() {
  return Serial.read();
}
