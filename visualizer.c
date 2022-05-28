#include <SDL2/SDL.h>
#include <fftw3.h>

#include "visualizer.h"

static SDL_Window* gWindow = NULL;
static SDL_Renderer* gRenderer = NULL;

static SDL_AudioDeviceID gAudioDeviceID = 0;
static SDL_AudioSpec gAudioSpec;

static fftw_plan fft_audio_plan[2];
static double fft_audio_data_in[2][VIZ_NUM_SAMPLES];
static fftw_complex fft_audio_data_out[2][VIZ_NUM_SAMPLES];

static double bar_levels[2][VIZ_NUM_BINS];

static uint8_t* audio_stream = NULL;
static uint8_t* audio_pos = NULL;
static uint32_t audio_len = 0;

static void draw_bar(int x, int width, int height) {
    SDL_Rect bar = { x, VIZ_WINDOW_YSIZE - height, width, height };

    SDL_SetRenderDrawColor(gRenderer, 255, 255, 255, 255);
    SDL_RenderFillRect(gRenderer, &bar);
}

static void draw_visualizer(void) {
    SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 0);
    SDL_RenderClear(gRenderer);

    int width = 20;
    int spacing = 5;

    for (int i = 0; i < VIZ_NUM_BINS; i++) {
        draw_bar(10 + (width + spacing) * i, width, VIZ_WINDOW_YSIZE * bar_levels[0][i]);
    }

    for (int i = 0; i < VIZ_NUM_BINS; i++) {
        int x = VIZ_WINDOW_XSIZE - (width + spacing) * i - width;
        draw_bar(x - 10, width, VIZ_WINDOW_YSIZE * bar_levels[1][i]);
    }

    SDL_RenderPresent(gRenderer);
}

static int init_sdl2_video(void) {
    gWindow = SDL_CreateWindow(
        VIZ_WINDOW_TITLE, VIZ_WINDOW_XPOS,VIZ_WINDOW_YPOS,
        VIZ_WINDOW_XSIZE, VIZ_WINDOW_YSIZE, 0
    );

    if (!gWindow) return -1;

    gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!gRenderer) return -1;

    return 0;
}

static double get_sample(uint8_t* stream) {
    SDL_AudioFormat format = gAudioSpec.format;

    uint16_t value = SDL_AUDIO_ISLITTLEENDIAN(format) ?
        (uint16_t)((stream[1] << 8) | stream[0]) :
        (uint16_t)(stream[0] | (stream[1] << 8));

    if (SDL_AUDIO_ISSIGNED(format)) {
        return (value - 32767) / 32768.0;
    }

    return value / 65535.0;
}

static void fill_fft_input(uint8_t* stream) {
    for (int j = 0; j < gAudioSpec.channels; j++) {
        for (int i = 0; i < VIZ_NUM_SAMPLES; i++) {
            double multiplier = 0.5 * (1.0 - cos(2.0 * M_PI * (double)i / (double)(VIZ_NUM_SAMPLES - 1)));
            fft_audio_data_in[j][i] = get_sample(stream + VIZ_NUM_SAMPLES * j) * multiplier;
            stream += 2;
        }
    }
}

static void calculate_bar_levels(void) {
    const double smoothing = pow(0.0007, (double)VIZ_NUM_SAMPLES / (double)VIZ_SAMPLE_RATE);
    double freq_bins[VIZ_NUM_BINS + 1];

    for (int i = 0; i <= VIZ_NUM_BINS; i++) {
        double j = powf((double)(i) / (double)(VIZ_NUM_BINS), 1.5);
        freq_bins[i] = (int)round(j * (double)VIZ_NUM_SAMPLES / 2.0);
    }

    for (int j = 0; j < gAudioSpec.channels; j++) {
        for (int i = 0; i < VIZ_NUM_BINS; i++) {
            int start_freq = freq_bins[i];
            int end_freq = freq_bins[i + 1];

            double modulus = 0.0;

            for (int i = start_freq; i < end_freq; i++) {
                double re = fft_audio_data_out[j][i][0];
                double im = fft_audio_data_out[j][i][1];

                double p = re * re + im * im;

                if (p > modulus) modulus = p;
            }
            modulus = log(modulus);
            if (modulus < 0.0) modulus = 0.0;

            bar_levels[j][i] = bar_levels[j][i] * smoothing + modulus *  0.05 * (1.0f - smoothing);
        }
    }
}

static void process_audio(void* udata, uint8_t* stream, int len) {
    len = viz_min(audio_len, len);

    fill_fft_input(audio_pos);
    fftw_execute(fft_audio_plan[0]);
    fftw_execute(fft_audio_plan[1]);
    calculate_bar_levels();

    memcpy(stream, audio_pos, len);

    audio_len -= len;
    audio_pos += len;
}

static void load_audio_file(const char* fn) {
    uint8_t* new_audio_stream;
    uint32_t new_audio_len;
    SDL_AudioSpec wav_spec; 

    if (SDL_LoadWAV(fn, &wav_spec, &new_audio_stream, &new_audio_len) == NULL) return;
    if (audio_stream) SDL_FreeWAV(audio_stream);

    if (wav_spec.channels != 1 && wav_spec.channels != 2) {
        fprintf(stderr, "Illegal number of channels.\n");
        SDL_FreeWAV(new_audio_stream);
        return;
    }

    if (gAudioDeviceID != 0) SDL_CloseAudioDevice(gAudioDeviceID);

    wav_spec.samples = VIZ_NUM_SAMPLES;
    wav_spec.callback = process_audio;
    gAudioDeviceID = SDL_OpenAudioDevice(NULL, 0, &wav_spec, NULL, 0);
    gAudioSpec = wav_spec;

    audio_stream = new_audio_stream;
    audio_pos = new_audio_stream;
    audio_len = new_audio_len;
    memset(bar_levels, 0, sizeof(double) * VIZ_NUM_BINS * 2);
    SDL_PauseAudioDevice(gAudioDeviceID, 0);
}

static int init_fft(void) {
    fft_audio_plan[0] = fftw_plan_dft_r2c_1d(VIZ_NUM_SAMPLES, fft_audio_data_in[0], fft_audio_data_out[0], FFTW_MEASURE);
    fft_audio_plan[1] = fftw_plan_dft_r2c_1d(VIZ_NUM_SAMPLES, fft_audio_data_in[1], fft_audio_data_out[1], FFTW_MEASURE);
    return (fft_audio_plan[0] && fft_audio_plan[1]) ? 0 : 1;
}

static void destroy_fft(void) {
    fftw_destroy_plan(fft_audio_plan[0]);
    fftw_destroy_plan(fft_audio_plan[1]);
    fftw_cleanup();
}

static void destroy_sdl_video(void) {
    SDL_DestroyRenderer(gRenderer);
    SDL_DestroyWindow(gWindow);
}

static void destroy_sdl_audio(void) {
    if (audio_stream) SDL_FreeWAV(audio_stream);
    SDL_CloseAudioDevice(gAudioDeviceID);
}

static void main_loop(void) {
    SDL_Event ev;
    int is_running = 1;

    while (is_running) {
        SDL_PollEvent(&ev);

        switch (ev.type) {
            case SDL_QUIT:
                is_running = 0;
                break;

            case SDL_DROPFILE:
                load_audio_file(ev.drop.file);
                break;
        }

        draw_visualizer();
    }
}

int main(int argc, char* argv[]) {
    if (SDL_Init(VIZ_SDL_INIT_FLAGS) != 0)
        raise_error("%s", SDL_GetError());

    if (init_sdl2_video() != 0) SDL_Quit(), raise_error("%s", SDL_GetError());
    if (init_fft() != 0) SDL_Quit(), raise_error("Could not create fftw plan.");

    memset(bar_levels, 0, sizeof(double) * VIZ_NUM_BINS * 2);

    main_loop();

    destroy_sdl_video();
    destroy_sdl_audio();
    destroy_fft();
    SDL_Quit();
}
