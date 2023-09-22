#pragma once

#include <cstdint>

class MandelbrotApp;

// Off-chip resource.
using TextureHandle = std::uint32_t;

class MandelbrotView {
public:
  // Assumes initialized GL context.
  MandelbrotView(const MandelbrotApp& parent);

  void render_iterates();
  void render_scene();

private:
  const MandelbrotApp& parent_;
  TextureHandle mandel_texture_;
};
