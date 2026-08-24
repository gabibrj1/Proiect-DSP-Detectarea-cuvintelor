#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>

static void write_u32(FILE *f, uint32_t v) { fwrite(&v, 4, 1, f); }
static void write_u16(FILE *f, uint16_t v) { fwrite(&v, 2, 1, f); }

int main(void) {
    const uint32_t sample_rate = 16000;
    const uint16_t num_channels = 1;
    const uint16_t bits_per_sample = 16;
    const double duration_sec = 3.0; // 1s ton, 1s tacere, 1s ton
    const uint32_t num_samples = (uint32_t)(sample_rate * duration_sec);

    int16_t *samples = malloc(num_samples * sizeof(int16_t));
    for (uint32_t i = 0; i < num_samples; i++) {
        double t = (double)i / sample_rate;
        if (t < 1.0 || t >= 2.0) {
            // ton 440 Hz, amplitudine moderata
            samples[i] = (int16_t)(8000.0 * sin(2.0 * M_PI * 440.0 * t));
        } else {
            samples[i] = 0; // tacere intre secunda 1 si 2
        }
    }

    FILE *f = fopen("data/test_tone.wav", "wb");
    uint32_t data_size = num_samples * num_channels * (bits_per_sample / 8);
    uint32_t byte_rate = sample_rate * num_channels * (bits_per_sample / 8);
    uint16_t block_align = num_channels * (bits_per_sample / 8);

    fwrite("RIFF", 1, 4, f);
    write_u32(f, 36 + data_size);
    fwrite("WAVE", 1, 4, f);

    fwrite("fmt ", 1, 4, f);
    write_u32(f, 16);
    write_u16(f, 1); // PCM
    write_u16(f, num_channels);
    write_u32(f, sample_rate);
    write_u32(f, byte_rate);
    write_u16(f, block_align);
    write_u16(f, bits_per_sample);

    fwrite("data", 1, 4, f);
    write_u32(f, data_size);
    fwrite(samples, sizeof(int16_t), num_samples, f);

    fclose(f);
    free(samples);
    printf("Generat: data/test_tone.wav (%.1fs, %u Hz)\n", duration_sec, sample_rate);
    return 0;
}