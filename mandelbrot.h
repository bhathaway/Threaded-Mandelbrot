#pragma once
#include <complex>

class ComplexIterate {
public:
  typedef std::complex<double> ValueType;

public:
  ComplexIterate() = default;
  ComplexIterate(const ComplexIterate & c) = default;

  ComplexIterate(double r, double i)
  : _start(r, i), _value(_start), _slow_value(_start)
  { }

  void iterate();

  ValueType getValue() const { return _value; }

  float getCount() const
  {
    if (_escaped) {
      return _adjusted_count;
    } else {
      return _count;
    }
  }

  bool escaped() const { return _escaped; }

  bool bounded() const { return _bounded; }

private:
  ValueType _start;
  ValueType _value;
  ValueType _slow_value;
  bool _escaped = false;
  bool _bounded = false;
  unsigned _count = 0;
  double _adjusted_count;
};

class Pixel
{
public:
  typedef void (*ColorMapFunc)(bool, double, float&, float&, float&);

public:
  Pixel() = default;
  Pixel(double left, double top, double width);

  void iterate();

  bool isFinal() const { return _final; }

  void color(unsigned char& r, unsigned char& g, unsigned char& b)
  { r = _red; g = _green; b = _blue; }

  void computeColor(unsigned char& r, unsigned char& g, unsigned char& b);

private:
  static ColorMapFunc colorMap;
  static constexpr unsigned subsample_width = 2;
  static constexpr unsigned subsamples = 2 * subsample_width * subsample_width;

private:
  bool _final = false;
  // Subsampling for smoothness.
  ComplexIterate _sub_iterates[32];
  double _width;
  unsigned char _red;
  unsigned char _green;
  unsigned char _blue;
};
