#include <stdio.h>
#include <stdlib.h>
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
    BlockResult *results = malloc(max_blocks * sizeof(BlockResult));

    size_t num_blocks = compute_block_energy(audio.samples, audio.num_samples, block_size, results);

    // folosim primele 10 blocuri (200ms) ca sa estimam zgomotul de fond
    // pentru test_tone.wav asta nu e realist (fisierul incepe
    // direct cu ton, nu cu tacere), dar pe audio real cu tacere la
    // inceput asta functioneaza corect. pastram testul ca sa vedem
    // mecanismul in actiune, chiar daca pragul rezultat nu e ideal aici
    double threshold = compute_adaptive_threshold(results, num_blocks, 10, 3.0);
    printf("Prag adaptiv calculat: %.2f\n\n", threshold);

    // hangover de 5 blocuri = 100ms la block_size 320 / 16000Hz
    apply_adaptive_threshold(results, num_blocks, threshold, 5);

    int prev_voice = -1;
    for (size_t b = 0; b < num_blocks; b++) {
        if (results[b].is_voice != prev_voice) {
            double t = (double)(b * block_size) / audio.sample_rate;
            printf("t=%.3fs -> %s\n", t, results[b].is_voice ? "VOCE" : "TACERE");
            prev_voice = results[b].is_voice;
        }
    }

    free(results);
    wav_free(&audio);
    return 0;
}