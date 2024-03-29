#include "app.h"

#include <iostream>

MandelbrotApp::MandelbrotApp()
  : view_(*this)
{
  this->initialize(real_center_, imag_center_, real_width_);
}

void MandelbrotApp::process_next_bin()
{
  for (std::pair<int, int> p = *bin_queue_.pop();
       p != std::pair<int, int>(-1, -1);
       p = *bin_queue_.pop())
  {
    bool all_finished = true;
    unsigned y_start = p.second * bin_width_;
    unsigned x_start = p.first * bin_width_;
    for (unsigned y = y_start; y < y_start + bin_width_; ++y) {
      for (unsigned x = x_start; x < x_start + bin_width_; ++x) {
        Pixel& px = pixels_[y][x];
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
      bin_finished_[p.second][p.first] = true;
    }
  }
}

void MandelbrotApp::main_loop()
{
  const unsigned thread_count =
      std::min(std::thread::hardware_concurrency(), max_thread_count_);
  // TODO: Use a thread pool. This is needlessly expensive.
  std::vector<std::thread> threads(thread_count);

  for (unsigned y = 0; y < model_.window_height / bin_width_; ++y) {
    for (unsigned x = 0; x < model_.window_width / bin_width_; ++x) {
      if (!bin_finished_[y][x]) {
        bin_queue_.push(std::pair<unsigned, unsigned>(x, y));
      }
    }
  }
  for (unsigned i = 0; i < thread_count; ++i) {
    bin_queue_.push(std::pair<int, int>(-1, -1));
  }

  for (unsigned i = 0; i < thread_count; ++i) {
    threads[i] = std::thread(&MandelbrotApp::process_next_bin, this);
  }

  for (unsigned i = 0; i < thread_count; ++i) {
    threads[i].join();
  }
}

void MandelbrotApp::update_iterates(int x, int y)
{
  double new_x;
  double new_y;

  this->get_real_coord_from_screen(new_x, new_y, x, y);
  this->calculate_iterates(new_x, new_y);
}

void MandelbrotApp::zoom(int x, int y, double factor)
{
  double new_x;
  double new_y;

  this->get_real_coord_from_screen(new_x, new_y, x, y);

  double new_width = real_width_ * factor;
  this->initialize(new_x, new_y, new_width);
  real_center_ = new_x;
  imag_center_ = new_y;
  real_width_ = new_width;
}

void MandelbrotApp::get_real_coord_from_screen(double& real_x, double& real_y, double x, double y)
{
  constexpr double window_width = MandelbrotApp::Model::window_width;
  constexpr double window_height = MandelbrotApp::Model::window_height;

  double screen_x = (double)(x - (window_width / 2)) / ((double)window_width / 2.0);
  double screen_y = (double)((window_height / 2) - y) / ((double)window_height / 2.0);
  real_x = screen_x * (real_width_ / 2.0) + real_center_;
  real_y = screen_y * (real_width_ / 2.0) + imag_center_;
}

void MandelbrotApp::calculate_iterates(double x, double y)
{
  double& max_real = model_.iterate_window_data.max_real;
  double& min_real = model_.iterate_window_data.min_real;
  double& max_imag = model_.iterate_window_data.max_imag;
  double& min_imag = model_.iterate_window_data.min_imag;
  constexpr std::size_t iterate_limit = MandelbrotApp::IterateWindowData::iterate_limit;

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
  std::cout << "iterate_count: " << i << std::endl;
  std::cout << "Bounding box: " << "(" << min_real << ", " << min_imag << ")-("
    << max_real << ", " << max_imag << ")" << std::endl;
}

void MandelbrotApp::initialize(double real_center, double imag_center, double width)
{
  std::cout << "real: " << real_center << " imag: "
            << imag_center << " width: " << width << std::endl;

  double real_start = real_center - width / 2.0;
  double real;
  double imag = imag_center + width / 2.0;
  double real_inc = width / static_cast<double>(model_.window_width);
  double imag_inc = width / static_cast<double>(model_.window_height);

  unsigned x, y;
  for (y = 0; y < model_.window_height; ++y, imag -= imag_inc) {
    for (real = real_start, x = 0; x < model_.window_width; ++x,real += real_inc) {
      if (y % bin_width_ == 0 && x % bin_width_ == 0) {
        bin_finished_[y / bin_width_][x / bin_width_] = false;
      }
      new (&pixels_[y][x]) Pixel(real, imag, real_inc);
    }
  }
}
