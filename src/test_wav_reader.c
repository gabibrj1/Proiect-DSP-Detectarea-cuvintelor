#include <stdio.h>
#include "wav_reader.h"

int main(void) {
    WavAudio audio;

    if (wav_read("data/test_tone.wav", &audio) != 0) {
        fprintf(stderr, "Citire esuata\n");
        return 1;
    }

    printf("Sample rate: %u Hz\n", audio.sample_rate);
    printf("Canale: %u\n", audio.num_channels);
    printf("Biti/esantion: %u\n", audio.bits_per_sample);
    printf("Numar esantioane (per canal): %u\n", audio.num_samples);
    printf("Durata: %.2f secunde\n", (double)audio.num_samples / audio.sample_rate);

    printf("\nPrimele 10 esantioane:\n");
    for (int i = 0; i < 10 && i < (int)audio.num_samples; i++) {
        printf("  [%d] = %d\n", i, audio.samples[i]);
    }

    wav_free(&audio);
    return 0;
}