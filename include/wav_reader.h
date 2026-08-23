#ifndef WAV_READER_H
#define WAV_READER_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t sample_rate;
    uint16_t num_channels;
    uint16_t bits_per_sample;
    uint32_t num_samples;   // per canal
    int16_t *samples;       // buffer alocat dinamic, interleaved daca stereo
} WavAudio;

// Citeste un fisier .wav PCM 16-bit (mono sau stereo).
// Returneaza 0 la succes, -1 la eroare.
int wav_read(const char *filename, WavAudio *out);

// Elibereaza memoria alocata in out->samples.
void wav_free(WavAudio *audio);

#endif