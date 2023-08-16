#include "easy_serial.h"

std::string last_string_sent;

std::vector<std::string> easy_serial::split (const std::string &s, char delim)
{
    std::vector<std::string> result;
    std::stringstream ss (s);
    std::string item;
    while (getline (ss, item, delim))
    {
        result.push_back (item);
    }
    return result;
}
void easy_serial::send_byte(uint8_t b)
{
    if (this->is_connected == true)
    {
        try
        {
            uint8_t bytes[1];
            bytes[0] = b;
            this->serial.write(bytes, 1);
        }
        catch(...)
        {
            //do nothing
        }
    }
}
void easy_serial::send_string(std::string s)
{
    if (this->is_connected == true)
    {
        try
        {
            this->serial.write(s);
            last_string_sent = s;
        }
        catch(...)
        {
            //do nothing
        }
    }
}
void easy_serial::delay(int ms)
{
    unsigned long delay_timer = EasyRender::Millis();
    while((EasyRender::Millis() - delay_timer) < ms);
}
void easy_serial::resend()
{
    this->send_string(last_string_sent);
}
void easy_serial::tick()
{
    if (this->serial.isOpen())
    {
        this->is_connected = true;
        try
        {
            int bytes_available = this->serial.available();
            if (bytes_available > 0)
            {
                std::string read = this->serial.read(bytes_available);
                bool eat_byte = false;
                for (int x = 0; x < read.size(); x++)
                {
                    if (this->read_byte_handler != NULL)
                    {
                        eat_byte = this->read_byte_handler(read.at(x));
                    }
                    if (read.at(x) == '\n' || read.at(x) == '\r')
                    {
                        this->read_line.erase(std::remove(this->read_line.begin(), this->read_line.end(), '\n'),this->read_line.end());
                        this->read_line.erase(std::remove(this->read_line.begin(), this->read_line.end(), '\r'),this->read_line.end());
                        if (this->read_line != "")
                        {
                            if (this->read_line_handler != NULL) 
                            {
                                LOG_F(INFO, "Handling line %s", this->read_line.c_str());
                                this->read_line_handler(this->read_line);
                            }
                        }
                        this->read_line = "";
                    }
                    else
                    {
                        if (eat_byte == false)
                        {
                            this->read_line.push_back(read.at(x));
                        }
                    }
                }
            }
        }
        catch(...)
        {
            LOG_F(WARNING, "Device Disconnected: %s", this->serial_port.c_str());
            this->serial.close();
            this->serial_port = "";
            this->is_connected = false;
        }
    }
    else
    {
        this->is_connected = false;
        this->serial_port = "";
    }
    if (this->is_connected == false)
    {
        if ((EasyRender::Millis() - this->connection_retry_timer) > 1000)
        {
            this->connection_retry_timer = 0;
            //this->serial_port = "/dev/cu.usbmodem83201";
            if (this->serial_port == "")
            {
                std::vector<serial::PortInfo> devices_found = serial::list_ports();
                std::vector<serial::PortInfo>::iterator iter = devices_found.begin();
                while( iter != devices_found.end() )
                {
                    serial::PortInfo device = *iter++;
                    if (this->logged_devices_once == false) LOG_F(INFO, "Found Device-> %s - %s", device.port.c_str(), device.description.c_str());
                    std::transform(device.description.begin(), device.description.end(), device.description.begin(),[](unsigned char c){ return std::tolower(c); });
                    std::transform(connect_description.begin(), connect_description.end(), connect_description.begin(),[](unsigned char c){ return std::tolower(c); });
                    std::vector<std::string> multiple_descriptions = this->split (connect_description, '|');
                    for (int x = 0; x < multiple_descriptions.size(); x++)
                    {
                        if (device.description.find(multiple_descriptions[x]) != std::string::npos)
                        {
                            this->serial_port = device.port.c_str();
                            break;
                        }
                    }
                    this->logged_devices_once = true;
                }
            }
            if (this->serial_port != "")
            {
                try
                {
                    this->serial.setPort(this->serial_port.c_str());
                    this->serial.setBaudrate(this->baudrate);
                    auto timeout = serial::Timeout::simpleTimeout(2000);
                    this->serial.setTimeout(timeout);
                    this->serial.open();
                    if (this->serial.isOpen())
                    {
                        this->is_connected = true;
                        this->serial.setDTR(true);
                        this->delay(100);
                        this->serial.setDTR(false);
                        LOG_F(INFO,"Opened port: %s at %d baudrate", this->serial_port.c_str(), this->baudrate);
                    }
                    else
                    {
                        LOG_F(ERROR, "Could not open port! %s", this->serial_port.c_str());
                        this->is_connected = false;
                    }
                }
                catch (...)
                {
                    this->is_connected = false;
                    this->serial_port = "";
                }
            }
        }
    }
}