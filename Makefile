PLAT=$(shell uname)
CXX_FLAGS=-std=c++14 -O2 -Wall -Wextra -Werror -pedantic
smooth_mandel:
ifeq ($(PLAT),Darwin)
	g++ $(CXX_FLAGS) -o smooth_mandel smooth_mandel.cpp -pthread -L/System/Library/Frameworks \
	-framework GLUT -framework OpenGL
else
	g++ $(CXX_FLAGS) -o smooth_mandel smooth_mandel.cpp -lGL -lGLU -lglut -pthread
endif

clean:
	rm smooth_mandel
