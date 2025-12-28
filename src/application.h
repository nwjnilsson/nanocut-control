#ifndef APPLICATION_
#define APPLICATION_

#include <../include/config.h>
#include <algorithm>
#include <dirent.h>
#include <fstream>
#include <ftw.h>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#define DEFAULT_UNIT SCALE(1.f)

template <typename T> struct Point2 {
  T                       x;
  T                       y;
  static constexpr Point2 infPos()
  {
    return { std::numeric_limits<T>::infinity(),
             std::numeric_limits<T>::infinity() };
  }
  static constexpr Point2 infNeg()
  {
    return { -std::numeric_limits<T>::infinity(),
             -std::numeric_limits<T>::infinity() };
  }
};

template <typename T> struct Point3 {
  T                       x;
  T                       y;
  T                       z;
  static constexpr Point3 infPos()
  {
    return { std::numeric_limits<T>::infinity(),
             std::numeric_limits<T>::infinity() };
  }
  static constexpr Point3 infNeg()
  {
    return { -std::numeric_limits<T>::infinity(),
             -std::numeric_limits<T>::infinity() };
  }
};

using Point2f = Point2<float>;
using Point2d = Point2<double>;
using Point3f = Point3<float>;
using Point3d = Point3<double>;

template <typename T> static constexpr T inf()
{
  return std::numeric_limits<T>::infinity();
}

// Type-safe color system
struct Color4f {
  float r, g, b, a;
};

enum class Color {
  White,
  Black,
  Grey,
  LightRed,
  Red,
  LightGreen,
  Green,
  LightBlue,
  Blue,
  Yellow
};

// Constexpr color palette (values 0-255 range)
constexpr Color4f COLOR_PALETTE[] = {
  { 255.0f, 255.0f, 255.0f, 255.0f }, // White
  { 0.0f, 0.0f, 0.0f, 255.0f },       // Black
  { 100.0f, 100.0f, 100.0f, 255.0f }, // Grey
  { 190.0f, 0.0f, 0.0f, 255.0f },     // LightRed
  { 255.0f, 0.0f, 0.0f, 255.0f },     // Red
  { 0.0f, 190.0f, 0.0f, 255.0f },     // LightGreen
  { 0.0f, 255.0f, 0.0f, 255.0f },     // Green
  { 0.0f, 0.0f, 190.0f, 255.0f },     // LightBlue
  { 0.0f, 0.0f, 255.0f, 255.0f },     // Blue
  { 255.0f, 255.0f, 0.0f, 255.0f }    // Yellow
};

// Helper function to get color from enum
constexpr Color4f getColor(Color color)
{
  return COLOR_PALETTE[static_cast<int>(color)];
}

// Forward declarations moved to NcApp.h as needed

#endif // APPLICATION_
