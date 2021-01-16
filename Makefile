PLAT=$(shell uname)

smooth_mandel:
ifeq ($(PLAT),Darwin)
	g++ -std=c++11 -O2 -o smooth_mandel smooth_mandel.cpp -pthread -L/System/Library/Frameworks -framework GLUT -framework OpenGL
else
	g++ -std=c++11 -O2 -o smooth_mandel smooth_mandel.cpp -lGL -lGLU -lglut -pthread
endif

