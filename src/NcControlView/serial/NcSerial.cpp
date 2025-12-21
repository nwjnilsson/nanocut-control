#include "NcSerial.h"

std::string last_string_sent;

std::vector<std::string> NcSerial::split(const std::string& s, char delim)
{
  std::vector<std::string> result;
  std::stringstream        ss(s);
  std::string              item;
  while (getline(ss, item, delim)) {
    result.push_back(item);
  }
  return result;
}
void NcSerial::sendByte(uint8_t b)
{
  if (m_is_connected == true) {
    try {
      uint8_t bytes[1];
      bytes[0] = b;
      m_serial.write(bytes, 1);
    }
    catch (...) {
      // do nothing
    }
  }
}
void NcSerial::sendString(const std::string& s)
{
  if (m_is_connected == true) {
    try {
      m_serial.write(s);
      last_string_sent = s;
    }
    catch (...) {
      // do nothing
    }
  }
}
void NcSerial::delay(int ms)
{
  unsigned long delay_timer = NcRender::millis();
  while ((NcRender::millis() - delay_timer) < ms)
    ;
}
void NcSerial::resend() { sendString(last_string_sent); }
void NcSerial::tick()
{
  if (m_serial.isOpen()) {
    m_is_connected = true;
    try {
      int bytes_available = m_serial.available();
      if (bytes_available > 0) {
        std::string read = m_serial.read(bytes_available);
        bool        eat_byte = false;
        for (int x = 0; x < read.size(); x++) {
          if (m_read_byte_handler != NULL) {
            eat_byte = m_read_byte_handler(read.at(x));
          }
          if (read.at(x) == '\n' || read.at(x) == '\r') {
            m_read_line.erase(
              std::remove(m_read_line.begin(), m_read_line.end(), '\n'),
              m_read_line.end());
            m_read_line.erase(
              std::remove(m_read_line.begin(), m_read_line.end(), '\r'),
              m_read_line.end());
            if (m_read_line != "") {
              if (m_read_line_handler != NULL) {
                DLOG_F(INFO, "Handling line %s", m_read_line.c_str());
                m_read_line_handler(m_read_line);
              }
            }
            m_read_line = "";
          }
          else {
            if (eat_byte == false) {
              m_read_line.push_back(read.at(x));
            }
          }
        }
      }
    }
    catch (...) {
      LOG_F(WARNING, "Device Disconnected: %s", m_serial_port.c_str());
      m_serial.close();
      m_serial_port = "";
      m_is_connected = false;
    }
  }
  else {
    m_is_connected = false;
    m_serial_port = "";
  }
  if (m_is_connected == false) {
    if ((NcRender::millis() - m_connection_retry_timer) > 1000) {
      m_connection_retry_timer = 0;
      // m_serial_port = "/dev/cu.usbmodem83201";
      if (m_serial_port == "") {
        std::vector<serial::PortInfo> devices_found = serial::list_ports();
        std::vector<serial::PortInfo>::iterator iter = devices_found.begin();
        while (iter != devices_found.end()) {
          serial::PortInfo device = *iter++;
          if (m_logged_devices_once == false)
            LOG_F(INFO,
                  "Found Device-> %s - %s",
                  device.port.c_str(),
                  device.description.c_str());
          std::transform(device.description.begin(),
                         device.description.end(),
                         device.description.begin(),
                         [](unsigned char c) { return std::tolower(c); });
          std::transform(m_connect_description.begin(),
                         m_connect_description.end(),
                         m_connect_description.begin(),
                         [](unsigned char c) { return std::tolower(c); });
          std::vector<std::string> multiple_descriptions =
            this->split(m_connect_description, '|');
          for (int x = 0; x < multiple_descriptions.size(); x++) {
            if (device.description.find(multiple_descriptions[x]) !=
                std::string::npos) {
              m_serial_port = device.port.c_str();
              break;
            }
          }
          m_logged_devices_once = true;
        }
      }
      if (m_serial_port != "") {
        try {
          m_serial.setPort(m_serial_port.c_str());
          m_serial.setBaudrate(m_baudrate);
          auto timeout = serial::Timeout::simpleTimeout(2000);
          m_serial.setTimeout(timeout);
          m_serial.open();
          if (m_serial.isOpen()) {
            m_is_connected = true;
            m_serial.setDTR(true);
            this->delay(100);
            m_serial.setDTR(false);
            LOG_F(INFO,
                  "Opened port: %s at %d baudrate",
                  m_serial_port.c_str(),
                  m_baudrate);
          }
          else {
            LOG_F(ERROR, "Could not open port! %s", m_serial_port.c_str());
            m_is_connected = false;
          }
        }
        catch (...) {
          m_is_connected = false;
          m_serial_port = "";
        }
      }
    }
  }
}
