#ifndef APPLICATION_
#define APPLICATION_

#include <../include/config.h>
#include <algorithm>
#include <fstream>
#include <ftw.h>
#include <imgui.h>
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
             std::numeric_limits<T>::infinity(),
             std::numeric_limits<T>::infinity() };
  }
  static constexpr Point3 infNeg()
  {
    return { -std::numeric_limits<T>::infinity(),
             -std::numeric_limits<T>::infinity(),
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

  Color4f& operator=(const ImVec4& col)
  {
    r = col.x * 255.0f;
    g = col.y * 255.0f;
    b = col.z * 255.0f;
    a = col.w * 255.0f;
    return *this;
  }
  operator ImVec4() const
  {
    return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
  }
};

// Forward declarations moved to NcApp.h as needed

#endif // APPLICATION_
