#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "wav_reader.h"
#include "energy_detector.h"

// masoara timpul scurs intre doua momente, in milisecunde
static double elapsed_ms(struct timespec start, struct timespec end) {
    double sec = (double)(end.tv_sec - start.tv_sec);
    double nsec = (double)(end.tv_nsec - start.tv_nsec);
    return sec * 1000.0 + nsec / 1e6;
}

int main(int argc, char *argv[]) {
    const char *filename = (argc > 1) ? argv[1] : "data/test_tone.wav";
    int repeats = (argc > 2) ? atoi(argv[2]) : 100; // repetam de mai multe ori pt masuratori stabile

    WavAudio audio;
    if (wav_read(filename, &audio) != 0) {
        fprintf(stderr, "Citire esuata pentru %s\n", filename);
        return 1;
    }

    size_t block_size = 320;
    size_t max_blocks = audio.num_samples / block_size;
    BlockResult *results = malloc(max_blocks * sizeof(BlockResult));

    struct timespec t1, t2;

    // ---- benchmark scalar ----
    clock_gettime(CLOCK_MONOTONIC, &t1);
    for (int r = 0; r < repeats; r++) {
        compute_block_energy(audio.samples, audio.num_samples, block_size, results);
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);
    double time_scalar = elapsed_ms(t1, t2);

    // ---- benchmark NEON ----
    clock_gettime(CLOCK_MONOTONIC, &t1);
    for (int r = 0; r < repeats; r++) {
        compute_block_energy_neon(audio.samples, audio.num_samples, block_size, results);
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);
    double time_neon = elapsed_ms(t1, t2);

    printf("Fisier: %s\n", filename);
    printf("Durata semnal: %.2f s (%u esantioane)\n",
           (double)audio.num_samples / audio.sample_rate, audio.num_samples);
    printf("Numar blocuri: %zu, repetitii: %d\n\n", max_blocks, repeats);

    printf("Scalar: %.3f ms total, %.5f ms/rulare\n", time_scalar, time_scalar / repeats);
    printf("NEON:   %.3f ms total, %.5f ms/rulare\n", time_neon, time_neon / repeats);
    printf("\nSpeedup: %.2fx\n", time_scalar / time_neon);

    free(results);
    wav_free(&audio);
    return 0;
}