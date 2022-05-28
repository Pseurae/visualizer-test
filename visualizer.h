#ifndef VISUALIZER_H
#define VISUALIZER_H

#include <stdlib.h>

#define raise_error(...) \
    (fprintf(stderr, __VA_ARGS__), exit(EXIT_FAILURE))

#define viz_max(a, b) (a > b ? a : b)
#define viz_min(a, b) (a < b ? a : b)

#define VIZ_SDL_INIT_FLAGS (SDL_INIT_AUDIO | SDL_INIT_VIDEO)

#define VIZ_WINDOW_TITLE "Visualizer"

#define VIZ_WINDOW_XPOS (SDL_WINDOWPOS_CENTERED)
#define VIZ_WINDOW_YPOS (SDL_WINDOWPOS_CENTERED)

#define VIZ_WINDOW_XSIZE 800
#define VIZ_WINDOW_YSIZE 400

#define VIZ_SAMPLE_RATE 48000
#define VIZ_NUM_SAMPLES 512

#define VIZ_NUM_BINS 15

#endif /* VISUALIZER_H */