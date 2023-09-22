#include "app.h"

#include <iostream>

using namespace std;

MandelbrotApp::MandelbrotApp()
  : view_(*this)
{
  initialize(real_center, imag_center, real_width);
}

void MandelbrotApp::doBin()
{
  for (pair<int, int> p = *bin_queue.pop(); p != pair<int, int>(-1, -1); p = *bin_queue.pop()) {
    bool all_finished = true;
    unsigned y_start = p.second * bin_width;
    unsigned x_start = p.first * bin_width;
    for (unsigned y = y_start; y < y_start + bin_width; ++y) {
      for (unsigned x = x_start; x < x_start + bin_width; ++x) {
        Pixel& px = pixels[y][x];
        px.iterate();
        unsigned char& r = model_.texture_data[model_.window_width*3*y + 3*x + 0];
        unsigned char& g = model_.texture_data[model_.window_width*3*y + 3*x + 1];
        unsigned char& b = model_.texture_data[model_.window_width*3*y + 3*x + 2];
        px.color(r, g, b);
        if (!px.isFinal()) {
          all_finished = false;
        }
      }
    }
    if (all_finished) {
      bin_finished[p.second][p.first] = true;
    }
  }
}

void MandelbrotApp::main_loop()
{
  // TODO: Use a thread pool. This is needlessly expensive.
  thread threads[thread_count];

  for (unsigned y = 0; y < model_.window_height / bin_width; ++y) {
    for (unsigned x = 0; x < model_.window_width / bin_width; ++x) {
      if (!bin_finished[y][x]) {
        bin_queue.push(pair<unsigned, unsigned>(x, y));
      }
    }
  }
  for (unsigned i = 0; i < thread_count; ++i) {
    bin_queue.push(pair<int, int>(-1, -1));
  }

  for (unsigned i = 0; i < thread_count; ++i) {
    threads[i] = thread(&MandelbrotApp::doBin, this);
  }

  for (unsigned i = 0; i < thread_count; ++i) {
    threads[i].join();
  }

  ++iteration;
  if (iteration % 100 == 0) {
    cout << "iteration " << iteration << endl;
  }
}

void MandelbrotApp::update_iterates(int x, int y)
{
  double new_x;
  double new_y;

  get_real_coord_from_screen(new_x, new_y, x, y);

  calculate_iterates(new_x, new_y);
}

void MandelbrotApp::zoom(int x, int y, double factor)
{
  double new_x;
  double new_y;

  get_real_coord_from_screen(new_x, new_y, x, y);

  double new_width = real_width * factor;
  initialize(new_x, new_y, new_width);
  real_center = new_x;
  imag_center = new_y;
  real_width = new_width;
}

void MandelbrotApp::get_real_coord_from_screen(double& real_x, double& real_y, double x, double y)
{
  constexpr double window_width = model_.window_width;
  constexpr double window_height = model_.window_height;

  double screen_x = (double)(x - (window_width / 2)) / ((double)window_width / 2.0);
  double screen_y = (double)((window_height / 2) - y) / ((double)window_height / 2.0);
  real_x = screen_x * (real_width / 2.0) + real_center;
  real_y = screen_y * (real_width / 2.0) + imag_center;
}

void MandelbrotApp::calculate_iterates(double x, double y)
{
  double& max_real = model_.iterate_window_data.max_real;
  double& min_real = model_.iterate_window_data.min_real;
  double& max_imag = model_.iterate_window_data.max_imag;
  double& min_imag = model_.iterate_window_data.min_imag;
  constexpr std::size_t iterate_limit = model_.iterate_window_data.iterate_limit;

  std::cout << "Calculating for (" << x << ", " << y << ")\n";
  new (&model_.iterates[0]) ComplexIterate(x, y);
  bool escaped = false;
  unsigned i;
  for (i = 0; !escaped && i < iterate_limit - 1; ++i) {
      double re = model_.iterates[i].getValue().real();
      double im = model_.iterates[i].getValue().imag();

      if (re < min_real) {
          min_real = re;
          if (min_real < -2.0) {
              min_real = -2.0;
          }
      } else if (re > max_real) {
          max_real = re;
          if (max_real > 2.0) {
              max_real = 2.0;
          }
      }

      if (im < min_imag) {
          min_imag = im;
          if (min_imag < -2.0) {
              min_imag = -2.0;
          }
      } else if (im > max_imag) {
          max_imag = im;
          if (max_imag > 2.0) {
              max_imag = 2.0;
          }
      }
      // Copy constructor leaves state as is.
      new (&model_.iterates[i + 1]) ComplexIterate(model_.iterates[i]);
      model_.iterates[i + 1].iterate();
      escaped = model_.iterates[i + 1].escaped();
  }
  model_.iterate_window_data.iterate_count = i;
  cout << "iterate_count: " << i << endl;
  cout << "Bounding box: " << "(" << min_real << ", " << min_imag << ")-("
    << max_real << ", " << max_imag << ")" << endl;
}

void MandelbrotApp::initialize(double real_center, double imag_center, double width)
{
  cout << "real: " << real_center << " imag: " << imag_center << " width: " << width << endl;

  double real_start = real_center - width / 2.0;
  double real;
  double imag = imag_center + width / 2.0;
  double real_inc = width / static_cast<double>(model_.window_width);
  double imag_inc = width / static_cast<double>(model_.window_height);

  unsigned x, y;
  for (y = 0; y < model_.window_height; ++y, imag -= imag_inc) {
    for (real = real_start, x = 0; x < model_.window_width; ++x,real += real_inc) {
      if (y % bin_width == 0 && x % bin_width == 0) {
        bin_finished[y / bin_width][x / bin_width] = false;
      }
      new (&pixels[y][x]) Pixel(real, imag, real_inc);
    }
  }

  iteration = 0;
}
