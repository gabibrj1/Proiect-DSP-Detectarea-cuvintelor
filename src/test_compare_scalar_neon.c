#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "wav_reader.h"
#include "energy_detector.h"

int main(void) {
    WavAudio audio;
    if (wav_read("data/test_tone.wav", &audio) != 0) {
        fprintf(stderr, "Citire esuata\n");
        return 1;
    }

    size_t block_size = 320;
    size_t max_blocks = audio.num_samples / block_size;

    BlockResult *results_scalar = malloc(max_blocks * sizeof(BlockResult));
    BlockResult *results_neon   = malloc(max_blocks * sizeof(BlockResult));

    size_t n1 = compute_block_energy(audio.samples, audio.num_samples, block_size, results_scalar);
    size_t n2 = compute_block_energy_neon(audio.samples, audio.num_samples, block_size, results_neon);

    if (n1 != n2) {
        printf("EROARE: numar diferit de blocuri (%zu vs %zu)\n", n1, n2);
        return 1;
    }

    double max_diff = 0.0;
    int mismatches = 0;

    for (size_t b = 0; b < n1; b++) {
        double diff = fabs(results_scalar[b].energy - results_neon[b].energy);
        if (diff > max_diff) max_diff = diff;
        if (diff > 1e-6 * results_scalar[b].energy) {
            mismatches++;
            if (mismatches <= 5) {
                printf("Diferenta bloc %zu: scalar=%.4f neon=%.4f (diff=%.6f)\n",
                       b, results_scalar[b].energy, results_neon[b].energy, diff);
            }
        }
    }

    printf("\nTotal blocuri: %zu\n", n1);
    printf("Diferenta maxima absoluta: %.10f\n", max_diff);
    printf("Blocuri cu diferenta semnificativa: %d\n", mismatches);
    printf(mismatches == 0 ? "REZULTAT: identic (validare OK)\n"
                            : "REZULTAT: diferente detectate!\n");

    free(results_scalar);
    free(results_neon);
    wav_free(&audio);
    return mismatches == 0 ? 0 : 1;
}