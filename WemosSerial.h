/*
 * This class offers a hardware serial which is not connected to the Wemos onboard CH430 chip
 * 
 * After trying several software serial solutions, with many stack dumps, I found a solution for using the second UART
 * After some research 
 * - UART0 can be configured for TX on TXD0(GPIO1) or D4(GPIO2) and RX on RXD0(GPIO3), or after swap TX/RX on D8,D7 (GPIO15/GPIO13)
 * - Swap UART0 will set TX to pin D8. However the wiring on D8 will pull the pin high and cause the ESP not to boot
 * - UART1 can be configured for TX on GPIO7 or D4(GPIO2) and RX on GPIO8, or after swap TX/RX on GPIO11/GPIO6
 * - RX pins are all used by flash chip and can not be accessed: UART1 can not be used for RX
 *   a forum states: "uart1 RX is multi-used for spi-flash" and not available
 *
  * So this library uses
 * - UART0 for RX on D7 (GPIO13)
 * - UART1 for TX on D4 (GPIO2)
 *
 * Be noted that the onboard LED, connected on D4, can no longer be used !
 */
#ifndef WEMOS_SERIAL
#define WEMOS_SERIAL

#include <HardwareSerial.h>

class WemosSerial : private HardwareSerial 
{
public:
  WemosSerial();
  // configure UART0 for RX, and UART1 for TX
  bool begin(int baudrate); 
  // serial output is redirected using UART1 on pin D4 (TXD1)
  bool print(const char*s);
  // serial input is redirected to Serial, using UART0 on pin D7 (RX02)
  int available();
  int read();
};

#endif
