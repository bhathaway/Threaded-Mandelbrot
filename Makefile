PLAT=$(shell uname)
CXX_FLAGS=-std=c++14 -O2 -Wall -Wextra -Werror -pedantic

smooth_mandel: smooth_mandel.cpp mandelbrot.cpp mandelbrot.h app.h app.cpp view.h view.cpp
ifeq ($(PLAT),Darwin)
	g++ $(CXX_FLAGS) -o smooth_mandel smooth_mandel.cpp mandelbrot.cpp app.cpp view.cpp -pthread \
	-L/System/Library/Frameworks -framework GLUT -framework OpenGL
else
	g++ $(CXX_FLAGS) -o smooth_mandel smooth_mandel.cpp mandelbrot.cpp app.cpp view.cpp \
	-lGL -lGLU -lglut -pthread
endif

clean:
	rm smooth_mandel
