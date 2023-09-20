#include <GL/glut.h>
#include <complex>
#include <iostream>
#include <thread>
#include "BlockingQueue.h"
#include <cmath>

using namespace std;

static double real_center = -0.85, imag_center = 0.0, width = 2.8;
const int window_width = 800;
const int window_height = 800;

int main_window, iterates_window;


double starting_epsilon;

class ComplexIterate
{
public:
    typedef complex<double> ValueType;

public:
    ComplexIterate()
    { }

    ComplexIterate(const ComplexIterate & c)
    : _start(c._start), _value(c._value), _slow_value(c._slow_value),
      _escaped(c._escaped), _bounded(c._bounded), _count(c._count)
    { }

    ComplexIterate(double r, double i)
    : _start(r, i), _value(_start), _slow_value(_start), _escaped(false),
      _bounded(false), _count(0)
    { }
    
    void iterate()
    {
        if (!_escaped && !_bounded) {
            _value = _value * _value + _start;
            auto abs_sqr = _value.real()*_value.real() +
            _value.imag()*_value.imag();
            if (abs_sqr > escape_value) {
                _escaped = true;
                _adjusted_count = _count -
                  log(log(abs_sqr) / log(escape_value))/log(2.0);
            } else {
                const double epsilon = 0.00000000000001;
                //double epsilon = starting_epsilon / (_count + 1);
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

    bool bounded() const
    {
        return _bounded;
    }

    static double escape_value;

private:
    ValueType _start;
    ValueType _value;
    ValueType _slow_value;
    bool _escaped;
    bool _bounded;
    unsigned _count;
    double _adjusted_count;
};

// Most iterates to see in the iterate window
const size_t iterate_limit = 5000;
size_t iterate_count = 0;
ComplexIterate iterates[iterate_limit];
double min_real, max_real, min_imag, max_imag;

void calculateIterates(double x, double y)
{
    min_real = x;
    max_real = x;
    min_imag = y;
    max_imag = y;

    cout << "Calculating for (" << x << ", " << y << ")\n";
    new (&iterates[0]) ComplexIterate(x, y);
    bool escaped = false;
    unsigned i;
    for (i = 0; !escaped && i < iterate_limit - 1; ++i) {
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
    glutPostWindowRedisplay(iterates_window);
}

double ComplexIterate::escape_value = 10E100;

const unsigned subsample_width = 2;
const unsigned subsamples = 2*subsample_width*subsample_width;

class Pixel
{
public:
    typedef void (*ColorMapFunc)(bool, double, float &, float &, float &);

public:
    Pixel() { }

    Pixel(double left, double top, double width)
    : _final(false), _left(left), _top(top), _width(width)
    {
        // Setup the sub-iterates.
        double sub_width = _width / static_cast<double>(subsample_width);
        double quarter_width = sub_width / 4.0;
        double three_width = sub_width / 2.0 + quarter_width;
        double x, y;
        unsigned i, k;
        unsigned iter = 0;
        for (x = left + quarter_width, i = 0; i < subsample_width;
        x += sub_width, ++i) {
            for (y = top - quarter_width, k = 0; k < subsample_width;
            y -= sub_width, ++k) {
                new (&_sub_iterates[iter]) ComplexIterate(x, y);
                ++iter;
            }
        }
        for (x = left + three_width, i = 0; i < subsample_width;
        x += sub_width, ++i) {
            for (y = top - three_width, k = 0; k < subsample_width;
            y -= sub_width, ++k) {
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

    bool isFinal() const
    {
        return _final;
    }

    void color(unsigned char & r, unsigned char & g, unsigned char & b)
    {
        r = _red; g = _green; b = _blue;
    }

    void computeColor(unsigned char & r, unsigned char & g, unsigned char & b)
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

    static ColorMapFunc colorMap;

private:
    bool _final;
    // Subsampling for smoothness.
    ComplexIterate _sub_iterates[32];
    double _left, _top, _width;
    unsigned char _red, _green, _blue;
};


void colorMap1(bool esc, double iter, float & r, float & g, float & b)
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


const int bin_width = 4;

bool bin_finished[window_height / bin_width][window_width / bin_width];

GLuint mandel_texture;
unsigned char texture_data[window_width * window_height * 3];

Pixel pixels[window_height][window_width];

const unsigned num_bins =
  (window_height / bin_width) * (window_width / bin_width);

const unsigned thread_count = 8;
BlockingQueue<pair<int, int>, num_bins + thread_count> bin_queue;

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

unsigned iteration = 0;
void idleFunc()
{
    thread threads[thread_count];

    for (int y = 0; y < window_height / bin_width; ++y) {
        for (int x = 0; x < window_width / bin_width; ++x) {
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

Pixel::ColorMapFunc Pixel::colorMap = colorMap1;

void initialize(double real_center, double imag_center, double width)
{
    cout << "real: " << real_center << " imag: " << imag_center
      << " width: " << width << endl;
    starting_epsilon = log(1.0+log(1.0 + 1.0/width)) / window_width;

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
            double screen_x = (double)(x - (window_width / 2)) /
              ((double)window_width / 2.0);
            double screen_y = (double)((window_height / 2) - y) /
              ((double)window_height / 2.0);
            double new_x = screen_x * (width / 2.0) + real_center;
            double new_y = screen_y * (width / 2.0) + imag_center;
            calculateIterates(new_x, new_y);
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

        double new_x = screen_x * (width / 2.0) + real_center;
        double new_y = screen_y * (width / 2.0) + imag_center;
        double new_width = width * factor;

        initialize(new_x, new_y, new_width);
        real_center = new_x;
        imag_center = new_y;
        width = new_width;
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
    
    initialize(real_center, imag_center, width);

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

