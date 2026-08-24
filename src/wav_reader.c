#include "wav_reader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t read_u32(FILE *f) {
    unsigned char b[4];
    fread(b, 1, 4, f);
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static uint16_t read_u16(FILE *f) {
    unsigned char b[2];
    fread(b, 1, 2, f);
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

int wav_read(const char *filename, WavAudio *out) {
    FILE *f = fopen(filename, "rb");
    if (!f) {
        fprintf(stderr, "Eroare: nu pot deschide %s\n", filename);
        return -1;
    }

    char chunk_id[5] = {0};
    fread(chunk_id, 1, 4, f);
    if (strcmp(chunk_id, "RIFF") != 0) {
        fprintf(stderr, "Eroare: fisier nu are header RIFF valid\n");
        fclose(f);
        return -1;
    }
    read_u32(f); // dimensiune fisier - 8, ignorat
    fread(chunk_id, 1, 4, f);
    if (strcmp(chunk_id, "WAVE") != 0) {
        fprintf(stderr, "Eroare: nu e format WAVE\n");
        fclose(f);
        return -1;
    }

    uint16_t audio_format = 0, num_channels = 0, bits_per_sample = 0;
    uint32_t sample_rate = 0;
    uint32_t data_size = 0;
    long data_offset = 0;

    // Parcurgem chunk-urile pana gasim "fmt " si "data"
    while (fread(chunk_id, 1, 4, f) == 4) {
        uint32_t chunk_size = read_u32(f);

        if (strncmp(chunk_id, "fmt ", 4) == 0) {
            audio_format = read_u16(f);
            num_channels = read_u16(f);
            sample_rate = read_u32(f);
            read_u32(f);          // byte rate, ignorat
            read_u16(f);          // block align, ignorat
            bits_per_sample = read_u16(f);
            long extra = chunk_size - 16;
            if (extra > 0) fseek(f, extra, SEEK_CUR);
        } else if (strncmp(chunk_id, "data", 4) == 0) {
            data_size = chunk_size;
            data_offset = ftell(f);
            fseek(f, chunk_size, SEEK_CUR);
        } else {
            fseek(f, chunk_size, SEEK_CUR); // sarim peste chunk necunoscut
        }

        if (chunk_size % 2 != 0) fseek(f, 1, SEEK_CUR); // padding byte
    }

    if (audio_format != 1 || bits_per_sample != 16) {
        fprintf(stderr, "Eroare: suportam doar PCM 16-bit necomprimat\n");
        fclose(f);
        return -1;
    }
    if (data_offset == 0) {
        fprintf(stderr, "Eroare: nu am gasit chunk-ul 'data'\n");
        fclose(f);
        return -1;
    }

    uint32_t total_samples = data_size / sizeof(int16_t); // total, toate canalele
    int16_t *samples = malloc(total_samples * sizeof(int16_t));
    if (!samples) {
        fprintf(stderr, "Eroare: alocare memorie esuata\n");
        fclose(f);
        return -1;
    }

    fseek(f, data_offset, SEEK_SET);
    size_t read_count = fread(samples, sizeof(int16_t), total_samples, f);
    fclose(f);

    if (read_count != total_samples) {
        fprintf(stderr, "Avertisment: citite %zu din %u esantioane asteptate\n",
                read_count, total_samples);
    }

    out->sample_rate = sample_rate;
    out->num_channels = num_channels;
    out->bits_per_sample = bits_per_sample;
    out->num_samples = read_count / num_channels;
    out->samples = samples;

    return 0;
}

void wav_free(WavAudio *audio) {
    if (audio && audio->samples) {
        free(audio->samples);
        audio->samples = NULL;
    }
}