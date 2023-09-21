#include <complex>
#include <iostream>
#include <thread>
#include <cmath>
#include <GL/glut.h>
#include "mandelbrot.h"
#include "BlockingQueue.h"

using namespace std;

struct IterateWindowData {
    // Maximum iterates to see in the iterate window
    static constexpr std::size_t iterate_limit = 5000;
    std::size_t iterate_count = 0;
    double min_real;
    double max_real;
    double min_imag;
    double max_imag;
};

// `iterates` must be allocated, but this function will use placement new on
// its elemenst.
IterateWindowData calculateIterates(ComplexIterate iterates[], double x, double y)
{
    IterateWindowData result;
    double& min_real = result.min_real;
    double& max_real = result.max_real;
    double& min_imag = result.min_imag;
    double& max_imag = result.max_imag;
    std::size_t& iterate_count = result.iterate_count;

    min_real = x;
    max_real = x;
    min_imag = y;
    max_imag = y;

    cout << "Calculating for (" << x << ", " << y << ")\n";
    new (&iterates[0]) ComplexIterate(x, y);
    bool escaped = false;
    unsigned i;
    for (i = 0; !escaped && i < result.iterate_limit - 1; ++i) {
        double re, im;
        re = iterates[i].getValue().real();
        im = iterates[i].getValue().imag();

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
        new (&iterates[i+1]) ComplexIterate(iterates[i]);
        iterates[i+1].iterate();
        escaped = iterates[i+1].escaped();
    }
    iterate_count = i;
    cout << "iterate_count: " << iterate_count << endl;
    cout << "Bounding box: " << "(" << min_real << ", " << min_imag << ")-("
      << max_real << ", " << max_imag << ")" << endl;

    return result;
}

// Application state
namespace {
using GlutWindow = int;
GlutWindow main_window;
GlutWindow iterates_window;

double real_center = -0.85;
double imag_center = 0.0;
double real_width = 2.8;

constexpr GLsizei window_width = 800;
constexpr GLsizei window_height = 800;

// Iterates view globals:
IterateWindowData iterate_window_data;
ComplexIterate iterates[IterateWindowData::iterate_limit];

// Main window related globals:
constexpr unsigned bin_width = 4;

bool bin_finished[window_height / bin_width][window_width / bin_width];

GLuint mandel_texture;
unsigned char texture_data[window_width * window_height * 3];

Pixel pixels[window_height][window_width];

constexpr unsigned num_bins = (window_height / bin_width) * (window_width / bin_width);

constexpr unsigned thread_count = 8;
BlockingQueue<pair<int, int>, num_bins + thread_count> bin_queue;

unsigned iteration = 0;
}

void doBin()
{
    for (pair<int, int> p = *bin_queue.pop(); p != pair<int, int>(-1, -1); p = *bin_queue.pop()) {
        bool all_finished = true;
        unsigned y_start = p.second * bin_width;
        unsigned x_start = p.first * bin_width;
        for (unsigned y = y_start; y < y_start + bin_width; ++y) {
            for (unsigned x = x_start; x < x_start + bin_width; ++x) {
                Pixel & px = pixels[y][x];
                px.iterate();
                unsigned char & r =
                  texture_data[window_width*3*y + 3*x + 0];
                unsigned char & g =
                  texture_data[window_width*3*y + 3*x + 1];
                unsigned char & b =
                  texture_data[window_width*3*y + 3*x + 2];
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

void idleFunc()
{
    // TODO: Use a thread pool. This is needlessly expensive.
    thread threads[thread_count];

    for (unsigned y = 0; y < window_height / bin_width; ++y) {
        for (unsigned x = 0; x < window_width / bin_width; ++x) {
            if (!bin_finished[y][x]) {
                bin_queue.push(pair<unsigned, unsigned>(x, y));
            }
        }
    }
    for (unsigned i = 0; i < thread_count; ++i) {
        bin_queue.push(pair<int, int>(-1, -1));
    }

    for (unsigned i = 0; i < thread_count; ++i) {
        threads[i] = thread(doBin);
    }

    for (unsigned i = 0; i < thread_count; ++i) {
        threads[i].join();
    }

    ++iteration;
    if (iteration % 100 == 0) {
        cout << "iteration " << iteration << endl;
    }
    glutPostWindowRedisplay(main_window);
}

void renderIterates()
{
    const double max_real = iterate_window_data.max_real;
    const double min_real = iterate_window_data.min_real;
    const double max_imag = iterate_window_data.max_imag;
    const double min_imag = iterate_window_data.min_imag;
    const std::size_t iterate_count = iterate_window_data.iterate_count;

    double scale, x_offset, y_offset;

    // Assuming square.. TODO: Use screen dimensions.
    if (max_real - min_real > max_imag - min_imag) {
        // ---
        // ---
        // Width dominates
        scale = (max_real-min_real)/1.8;
    } else {
        // ||
        // ||
        // Height dominates
        scale = (max_imag-min_imag)/1.8;
    }
    x_offset = (max_real+min_real) / 2.0;
    y_offset = (max_imag+min_imag) / 2.0;

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

void renderScene()
{
    glBindTexture(GL_TEXTURE_2D, mandel_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, window_width, window_height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, texture_data);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

    glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
		glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
		glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
		glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
    glEnd();
    glFlush();
}

void initialize(double real_center, double imag_center, double width)
{
    cout << "real: " << real_center << " imag: " << imag_center
      << " width: " << width << endl;

    double real_start = real_center - width / 2.0;
    double real, imag = imag_center + width / 2.0;
    double real_inc =
      width / static_cast<double>(window_width);
    double imag_inc =
      width / static_cast<double>(window_height);

    unsigned x, y;
    for (y = 0; y < window_height; ++y, imag -= imag_inc) {
        for (real = real_start, x = 0; x < window_width; ++x,real += real_inc) {
            if (y % bin_width == 0 && x % bin_width == 0) {
                bin_finished[y/bin_width][x/bin_width] = false;
            }
            new (&pixels[y][x]) Pixel(real, imag, real_inc);
        }
    }
    iteration = 0;
}

void mouseHandler(int button, int state, int x, int y)
{
    bool button_pressed = false;
    double factor;
    if (glutGetModifiers() & GLUT_ACTIVE_SHIFT) {
        if (button == GLUT_LEFT_BUTTON) {
            // TODO: Factor common code below.
            double screen_x = (double)(x - (window_width / 2)) /
              ((double)window_width / 2.0);
            double screen_y = (double)((window_height / 2) - y) /
              ((double)window_height / 2.0);
            double new_x = screen_x * (real_width / 2.0) + real_center;
            double new_y = screen_y * (real_width / 2.0) + imag_center;
            iterate_window_data = calculateIterates(iterates, new_x, new_y);
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
        // Get the real coordinates of x and y
        double screen_x = (double)(x - (window_width / 2)) /
          ((double)window_width / 2.0);
        double screen_y = (double)((window_height / 2) - y) /
          ((double)window_height / 2.0);

        double new_x = screen_x * (real_width / 2.0) + real_center;
        double new_y = screen_y * (real_width / 2.0) + imag_center;
        double new_width = real_width * factor;

        initialize(new_x, new_y, new_width);
        real_center = new_x;
        imag_center = new_y;
        real_width = new_width;
    }
}

int main(int argc, char * argv[])
{
    // init GLUT and create Window
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGBA);
    glutInitWindowPosition(100,100);
    glutInitWindowSize(window_width, window_height);
    main_window = glutCreateWindow("Gradual Mandelbrot Rendering");
    
    initialize(real_center, imag_center, real_width);

    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &mandel_texture);
    glutDisplayFunc(renderScene);
    glutIdleFunc(idleFunc);
    glutMouseFunc(mouseHandler);

    // Create additional window for looking at iterates.
    iterates_window = glutCreateWindow("Iterate View");
    glutDisplayFunc(renderIterates);

    // enter GLUT event processing cycle
    glutMainLoop();
}
