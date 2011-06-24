#include <GL/glut.h>
#include <complex>
#include <iostream>
#include <boost/thread.hpp>
#include "BlockingQueue.h"
#include <cmath>

using namespace std;

class ComplexIterate
{
public:
    typedef complex<double> ValueType;

public:
    ComplexIterate()
    { }

    ComplexIterate(double r, double i)
    : _start(r, i), _value(_start), _escaped(false), _count(0)
    { }
    
    void iterate()
    {
        if (!_escaped) {
            _value = _value * _value + _start;
            auto abs_sqr = _value.real()*_value.real() +
            _value.imag()*_value.imag();
            if (abs_sqr > escape_value) {
                _escaped = true;
                _adjusted_count = _count -
                  log(log(abs_sqr) / log(escape_value))/log(2.0);
            }
            ++_count;
        }
    }

    ValueType getValue() const
    {
        return _value;
    }

    float getCount() const
    {
        if (_escaped) {
            return _adjusted_count;
        } else {
            return _count;
        }
    }

    bool escaped() const
    {
        return _escaped;
    }

    static double escape_value;

private:
    ValueType _start;
    ValueType _value;
    bool _escaped;
    unsigned _count;
    double _adjusted_count;
};

double ComplexIterate::escape_value = 10E100;


class Pixel
{
public:
    typedef void (*ColorMapFunc)(bool, double, float &, float &, float &);

public:
    Pixel() { }

    Pixel(double left, double top, double width,
          float screen_x, float screen_y)
    : _left(left), _top(top), _width(width), _final(false),
      _screen_x(screen_x), _screen_y(screen_y)
    {
        // Setup the sub-iterates.
        double sub_width = _width / 4.0;
        double quarter_width = sub_width / 4.0;
        double three_width = sub_width / 2.0 + quarter_width;
        double x, y;
        unsigned i, k;
        unsigned iter = 0;
        for (x = left + quarter_width, i = 0; i < 4; x += sub_width, ++i) {
            for (y = top - quarter_width, k = 0; k < 4; y -= sub_width, ++k) {
                new (&_sub_iterates[iter]) ComplexIterate(x, y);
                ++iter;
            }
        }
        for (x = left + three_width, i = 0; i < 4; x += sub_width, ++i) {
            for (y = top - three_width, k = 0; k < 4; y -= sub_width, ++k) {
                new (&_sub_iterates[iter]) ComplexIterate(x, y);
                ++iter;
            }
        }
    }

    void iterate()
    {
        if (_final) {
            return;
        }

        bool any_live = false;
        for (unsigned i = 0; i < 32; ++i) {
            _sub_iterates[i].iterate();
            if (!_sub_iterates[i].escaped()) {
                any_live = true;
            }
        }
        computeColor(_red, _green, _blue);
        if (!any_live) {
            _final = true;
        }
    }

    bool isFinal() const
    {
        return _final;
    }

    // Assumes opengl is already in GL_POINTS mode.
    void draw()
    {
        glColor3f(_red, _green, _blue);
        glVertex2d(_screen_x, _screen_y);
    }
    
    void color(unsigned char & r, unsigned char & g, unsigned char & b)
    {
        r = _red; g = _green; b = _blue;
    }

    void computeColor(unsigned char & r, unsigned char & g, unsigned char & b)
    {
        float r_avg = 0.0, g_avg = 0.0, b_avg = 0.0;
        for (unsigned i = 0; i < 32; ++i) {
            float red, green, blue;
            colorMap(_sub_iterates[i].escaped(),
              _sub_iterates[i].getCount(), red, green, blue);
            r_avg += red / 32.0;
            g_avg += green / 32.0;
            b_avg += blue / 32.0;
        }
        r = r_avg * 255.0;
        g = g_avg * 255.0;
        b = b_avg * 255.0;
    }

    static ColorMapFunc colorMap;

private:
    bool _final;
    // Subsampling for smoothness.
    ComplexIterate _sub_iterates[32];
    double _left, _top, _width;
    float _screen_x, _screen_y;
    unsigned char _red, _green, _blue;
};


void colorMap1(bool esc, double iter, float & r, float & g, float & b)
{
    if (!esc) {
        r = 0.0; g = 0.0; b = 0.0;
        return;
    }

    const float r_start = 1.0, g_start = 0.4, b_start = 0.2;
    const float r_end = 0.0, g_end = 0.541, b_end = 0.722;
    
    const unsigned range = 50;

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

const unsigned window_width = 960;
const unsigned window_height = 960;

GLuint mandel_texture;
unsigned char texture_data[window_width * window_height * 3];

Pixel pixels[window_height][window_width];

BlockingQueue<unsigned> line_queue;

void doLine()
{
    try {
        while (true) {
            unsigned y = line_queue.pop();
            for (unsigned x = 0; x < window_width; ++x) {
                pixels[y][x].iterate();
            }
        }
    } catch (BlockingQueue<unsigned>::Shutdown & e) {
    }
}

void idleFunc()
{
    static unsigned time = 0;
    const unsigned thread_count = 20;
    boost::thread threads[thread_count];

    for (unsigned y = 0; y < window_height; ++y) {
        line_queue.push(y);
    }
    line_queue.shutdown();
    for (unsigned i = 0; i < thread_count; ++i) {
        threads[i] = boost::thread(doLine);
    }

    for (unsigned i = 0; i < thread_count; ++i) {
        threads[i].join();
    }

    ++time;
    cout << time << endl;
    glutPostRedisplay();
}

void renderScene()
{
    for (unsigned y = 0; y < window_height; ++y) {
        for (unsigned x = 0; x < window_width; ++x) {
            unsigned char & r =
              texture_data[window_width*3*y + 3*x + 0];
            unsigned char & g =
              texture_data[window_width*3*y + 3*x + 1];
            unsigned char & b =
              texture_data[window_width*3*y + 3*x + 2];
            pixels[y][x].color(r, g, b);
        }
    }

    glBindTexture(GL_TEXTURE_2D, mandel_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, 3, window_width, window_height, 0, GL_RGB,
                 GL_UNSIGNED_BYTE, texture_data);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

    glBegin(GL_QUADS);
		glTexCoord2f(0.0f, 0.0f); glVertex3f(-1.0f, -1.0f,  1.0f);
		glTexCoord2f(1.0f, 0.0f); glVertex3f( 1.0f, -1.0f,  1.0f);
		glTexCoord2f(1.0f, 1.0f); glVertex3f( 1.0f,  1.0f,  1.0f);
		glTexCoord2f(0.0f, 1.0f); glVertex3f(-1.0f,  1.0f,  1.0f);
    glEnd();
    glFlush();
}

Pixel::ColorMapFunc Pixel::colorMap = colorMap1;

int main(int argc, char * argv[])
{
    // init GLUT and create Window
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGBA);
    glutInitWindowPosition(100,100);
    glutInitWindowSize(window_width, window_height);
    glutCreateWindow("Gradual Mandelbrot Rendering");

    double real_start = -2.25;
    double real = -2.0, imag = 1.4;
    double real_width = 2.8;
    double imag_height = 2.8;
    double real_inc =
      real_width / static_cast<double>(window_width);
    double imag_inc =
      imag_height / static_cast<double>(window_height);

    float screen_x_start = -1.0;
    float screen_x = -1.0, screen_y = 1.0;
    float screen_width = 2.0;
    float screen_height = 2.0;
    float x_inc = screen_width / static_cast<float>(window_width);
    float y_inc = screen_height / static_cast<float>(window_height);

    unsigned x, y;
    for (y = 0; y < window_height; ++y, imag -= imag_inc, screen_y -= y_inc) {
        for (screen_x = screen_x_start, real = real_start, x = 0;
        x < window_width; ++x, real += real_inc, screen_x += x_inc) {
              new (&pixels[y][x]) Pixel(real, imag, real_inc,
                        screen_x + x_inc/2.0, screen_y + y_inc/2.0);
        }
    }

    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &mandel_texture);
    glutDisplayFunc(renderScene);
    glutIdleFunc(idleFunc);

    // enter GLUT event processing cycle
    glutMainLoop();
}

