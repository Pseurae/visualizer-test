CC		    := gcc
CFLAGS		:= -I. -O3

SDL_ARGS	:= -I/usr/local/Cellar/sdl2/2.0.22/include/ -lSDL2
FFTW_ARGS	:= -I/usr/local/include -L/usr/local/lib -lfftw3

all: visualizer.c visualizer.h
	$(CC) $(SDL_ARGS) $(FFTW_ARGS) $(CFLAGS) -o visualizer visualizer.c

clean:
	rm -f visualizer
	echo "Done."

.PHONY: all
.SILENT: clean
