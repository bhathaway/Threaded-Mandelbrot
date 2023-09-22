#include <iostream>
#include <GL/glut.h>
#include "app.h"

using namespace std;

namespace {
using GlutWindow = int;
GlutWindow main_window;
GlutWindow iterates_window;
std::unique_ptr<MandelbrotApp> app;
}

void idleFunc()
{
  app->main_loop();
  glutPostWindowRedisplay(main_window);
}

void mouseHandler(int button, int state, int x, int y)
{
  bool button_pressed = false;
  double factor = 1.0;

  if (glutGetModifiers() & GLUT_ACTIVE_SHIFT) {
    if (button == GLUT_LEFT_BUTTON) {
      app->update_iterates(x, y);
      glutPostWindowRedisplay(iterates_window);
    }
  } else {
      if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
          button_pressed = true;
          factor = 0.666667;
      } else if (button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN) {
          button_pressed = true;
          factor = 1.5;
      }
  }

  if (button_pressed) {
    app->zoom(x, y, factor);
  }
}

void render_scene() {
  app->view().render_scene();
}

void render_iterates() {
  app->view().render_iterates();
}

int main(int argc, char * argv[])
{
  app = std::make_unique<MandelbrotApp>();

  // init GLUT and create Window
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_SINGLE | GLUT_RGBA);
  glutInitWindowPosition(100,100);
  glutInitWindowSize(app->model().window_width, app->model().window_height);
  main_window = glutCreateWindow("Gradual Mandelbrot Rendering");
  
  glEnable(GL_TEXTURE_2D);
  glGenTextures(1, app->texture_handle());
  glutDisplayFunc(render_scene);
  glutIdleFunc(idleFunc);
  glutMouseFunc(mouseHandler);

  // Create additional window for looking at iterates.
  iterates_window = glutCreateWindow("Iterate View");
  glutDisplayFunc(render_iterates);

  // enter GLUT event processing cycle
  glutMainLoop();
}
