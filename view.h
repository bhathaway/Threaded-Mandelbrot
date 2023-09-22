#pragma once

class MandelbrotApp;

class MandelbrotView {
public:
  // Initialized GLUT context.
  MandelbrotView(const MandelbrotApp& parent)
    : parent_(parent)
  { }

  void render_iterates();
  void render_scene();

private:
  const MandelbrotApp& parent_;
};
