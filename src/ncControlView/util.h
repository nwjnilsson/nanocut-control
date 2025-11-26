#pragma once
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>

template<typename T, std::enable_if_t<std::is_same_v<T, float> || std::is_same_v<T, double>, int> = 0>
std::string to_fixed_string(T n, int d)
{
    std::stringstream stream;
    stream << std::fixed << std::setprecision(d) << n;
    return stream.str();
}
template <typename T> std::string to_string_with_precision(T a_value, int precision = 5)
{
    std::ostringstream out;
    out.precision(precision);
    out << std::fixed << a_value;
    return out.str();
}

template <typename T> std::string to_string_strip_zeros(T a_value)
{
    std::ostringstream oss;
    oss << std::setprecision(5) << std::noshowpoint << a_value;
    std::string str = oss.str();
    return oss.str();
}

float adc_sample_to_voltage(uint16_t val);
int voltage_to_adc_sample(float val);
