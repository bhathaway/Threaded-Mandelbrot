#pragma once
#include "view.h"
#include "mandelbrot.h"
#include <GL/gl.h>
#include "BlockingQueue.h"

// Given the size of object instances, I recommend heap allocation
// for this type.
class MandelbrotApp {
public:
	struct IterateWindowData {
			// Maximum iterates to see in the iterate window
			static constexpr std::size_t iterate_limit = 5000;
			std::size_t iterate_count = 0;
			double min_real;
			double max_real;
			double min_imag;
			double max_imag;
	};

  struct Model {
    static constexpr GLsizei window_width = 800;
    static constexpr GLsizei window_height = 800;
		IterateWindowData iterate_window_data;
    ComplexIterate iterates[IterateWindowData::iterate_limit];
    static constexpr std::size_t color_channels = 3;
    GLuint mandel_texture;
    std::uint8_t texture_data[window_width * window_height * color_channels];
  };

public: // ***INTERFACE***
  // This should initialize the view and control.
  MandelbrotApp();
  void main_loop();
  void update_iterates(int x, int y);
  void zoom(int x, int y, double factor);

  const Model& model() const { return model_; }
  // TODO: Find a way to move this to the view, along with the texture handle.
  GLuint* texture_handle() { return &model_.mandel_texture; }
  MandelbrotView& view() { return view_; }

private: // Helper methods
  // Sets up Pixels according to new bounds. Should be called whenever the extents change.
  void initialize(double real_center, double imag_center, double width);
  void calculate_iterates(double x, double y);
  void get_real_coord_from_screen(double& real_x, double& real_y, double x, double y);
  void doBin();

private:
  MandelbrotView view_;
  Model model_;

	double real_center = -0.85;
	double imag_center = 0.0;
	double real_width = 2.8;

	// Main window related globals:
	static constexpr unsigned bin_width = 4;

	bool bin_finished[Model::window_height / bin_width][Model::window_width  / bin_width];

	Pixel pixels[Model::window_height][Model::window_width];

	static constexpr unsigned num_bins =
    (Model::window_height / bin_width) * (Model::window_width / bin_width);

	static constexpr unsigned thread_count = 8;
	BlockingQueue<std::pair<int, int>, num_bins + thread_count> bin_queue;

	unsigned iteration = 0;
};
