#include "mandelbrot.h"
#include <cmath>

namespace {
void colorMap1(bool esc, double iter, float& r, float& g, float& b)
{
  if (!esc) {
    r = 0.0; g = 0.271; b = 0.361;
    return;
  }

  const float r_start = 1.0, g_start = 0.4, b_start = 0.2;
  const float r_end = 0.0, g_end = 0.541, b_end = 0.722;

  const unsigned range = 100;

  if (fmod((iter / range), 2) < 1) {
    iter = fmod(iter, range);
  } else {
    iter = range - fmod(iter, range);
  }

  auto alpha = iter / static_cast<float>(range);

  r = (1.0 - alpha) * r_start + alpha * r_end;
  g = (1.0 - alpha) * g_start + alpha * g_end;
  b = (1.0 - alpha) * b_start + alpha * b_end;
}
}

void ComplexIterate::iterate()
{
  if (!_escaped && !_bounded) {
    _value = _value * _value + _start;
    auto abs_sqr = _value.real()*_value.real() +
    _value.imag()*_value.imag();

    constexpr double escape_value = 10e100;
    if (abs_sqr > escape_value) {
      _escaped = true;
      _adjusted_count = _count - log(log(abs_sqr) / log(escape_value)) / log(2.0);
    } else {
      constexpr double epsilon = 0.00000000000001;
      const double real_diff = _value.real() - _slow_value.real();

      if (real_diff < epsilon && real_diff > -epsilon) {
        const double imag_diff = _value.imag() - _slow_value.imag();
        if (imag_diff < epsilon && imag_diff > -epsilon) {
          _bounded = true;
        }
      }
    }

    if (_count > 0 && _count % 2 == 0) {
      _slow_value = _slow_value * _slow_value + _start;
    }
    ++_count;
  }
}

Pixel::Pixel(double left, double top, double width)
: _left(left), _top(top), _width(width)
{
  // Setup the sub-iterates.
  const double sub_width = _width / static_cast<double>(subsample_width);
  const double quarter_width = sub_width / 4.0;
  const double three_width = sub_width / 2.0 + quarter_width;
  double x, y;
  unsigned i, k;
  unsigned iter = 0;

  for (x = left + quarter_width, i = 0; i < subsample_width; x += sub_width, ++i) {
    for (y = top - quarter_width, k = 0; k < subsample_width; y -= sub_width, ++k) {
      new (&_sub_iterates[iter]) ComplexIterate(x, y);
      ++iter;
    }
  }

  for (x = left + three_width, i = 0; i < subsample_width; x += sub_width, ++i) {
    for (y = top - three_width, k = 0; k < subsample_width; y -= sub_width, ++k) {
      new (&_sub_iterates[iter]) ComplexIterate(x, y);
      ++iter;
    }
  }
}

void Pixel::iterate()
{
  if (_final) {
    return;
  }

  bool any_live = false;
  for (unsigned i = 0; i < subsamples; ++i) {
    _sub_iterates[i].iterate();
    if (!_sub_iterates[i].escaped() && !_sub_iterates[i].bounded()) {
      any_live = true;
    }
  }

  computeColor(_red, _green, _blue);
  if (!any_live) {
    _final = true;
  }
}

void Pixel::computeColor(unsigned char& r, unsigned char& g, unsigned char& b)
{
  float r_sum = 0.0, g_sum = 0.0, b_sum = 0.0;
  for (unsigned i = 0; i < subsamples; ++i) {
    float red, green, blue;
    colorMap(_sub_iterates[i].escaped(), _sub_iterates[i].getCount(), red, green, blue);
    r_sum += red;
    g_sum += green;
    b_sum += blue;
  }

  r = r_sum * (255.0 / static_cast<double>(subsamples));
  g = g_sum * (255.0 / static_cast<double>(subsamples));
  b = b_sum * (255.0 / static_cast<double>(subsamples));
}

Pixel::ColorMapFunc Pixel::colorMap = colorMap1;
