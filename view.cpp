#include "view.h"
#include "app.h"

#include <GL/gl.h>

MandelbrotView::MandelbrotView(const MandelbrotApp& parent)
  : parent_(parent)
{
  glEnable(GL_TEXTURE_2D);
  glGenTextures(1, &mandel_texture_);
}

void MandelbrotView::render_iterates()
{
  const MandelbrotApp::IterateWindowData& iterate_window_data =
      parent_.model().iterate_window_data;
  const ComplexIterate* iterates = parent_.model().iterates;

  const double max_real = iterate_window_data.max_real;
  const double min_real = iterate_window_data.min_real;
  const double max_imag = iterate_window_data.max_imag;
  const double min_imag = iterate_window_data.min_imag;
  const std::size_t iterate_count = iterate_window_data.iterate_count;

  double scale;
  double x_offset;
  double y_offset;

  // Assuming square.. TODO: Use screen dimensions.
  if (max_real - min_real > max_imag - min_imag) {
    // ---
    // ---
    // Width dominates
    scale = (max_real - min_real) / 1.8;
  } else {
    // ||
    // ||
    // Height dominates
    scale = (max_imag - min_imag) / 1.8;
  }
  x_offset = (max_real + min_real) / 2.0;
  y_offset = (max_imag + min_imag) / 2.0;

  glClear(GL_COLOR_BUFFER_BIT);
  glColor3f(1.0, 1.0, 1.0);
  glBegin(GL_POINTS);
  for (unsigned i = 0; i < iterate_count; ++i) {
    double re = iterates[i].getValue().real();
    double im = iterates[i].getValue().imag();
    double gl_x = (re - x_offset) / scale;
    double gl_y = (im - y_offset) / scale;
    // Convert to gl coordinates based on bounding box
    glVertex2f(gl_x, gl_y);
  }
  glEnd();
}

void MandelbrotView::render_scene()
{
  glBindTexture(GL_TEXTURE_2D, mandel_texture_);
  constexpr GLint level = 0;
  constexpr GLint border = 0;
  glTexImage2D(GL_TEXTURE_2D, level, GL_RGB8,
               parent_.model().window_width, parent_.model().window_height,
               border, GL_RGB,
               GL_UNSIGNED_BYTE, parent_.model().texture_data);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
  glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
  glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
  glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
  glEnd();
  glFlush();
}
