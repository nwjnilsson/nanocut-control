#ifndef NC_SERIAL_
#define NC_SERIAL_

#include <serial/serial.h>
#include <NcRender/NcRender.h>
#include <nlohmann/json.hpp>
#include <loguru.hpp>
#include <functional>
#include <iostream>
#include <stdio.h>

class NcSerial {
public:
  serial::Serial                   m_serial;
  std::string                      m_connect_description;
  bool                             m_auto_connect;
  int                              m_baudrate;
  bool                             m_is_connected;
  std::string                      m_serial_port;
  std::function<void(std::string)> m_read_line_handler;
  std::function<bool(uint8_t)> m_read_byte_handler; // returns true if byte is
                                                    // to be left off line and
                                                    // false if it is to be
                                                    // included!
  std::vector<std::string> split(const std::string& s, char delim);
  NcSerial(std::string                      c,
           std::function<bool(uint8_t)>     b,
           std::function<void(std::string)> r)
  {
    m_connect_description = c;
    m_auto_connect = true;
    m_baudrate = 115200;
    m_read_line_handler = r;
    m_read_byte_handler = b;
    m_connection_retry_timer = 0;
    m_serial_port = "";
    m_logged_devices_once = false;
  }
  /*
      access to main loop
  */
  void tick();

  /*
      Send a single byte
  */
  void sendByte(uint8_t b);

  /*
      Send a un-encoded string
  */
  void sendString(const std::string& s);

  /*
      Equivilent to arduino delay
  */
  void delay(int ms);

  /*
     Re-send last sent un-encoded string
  */
  void resend();

private:
  std::string   m_read_line;
  unsigned long m_connection_retry_timer;
  bool          m_logged_devices_once;
/*
    CRC-32C (iSCSI) polynomial in reversed bit order.
*/
#define POLY 0x82f63b78
  uint32_t crc32c(uint32_t crc, const char* buf, size_t len);
};

#endif
