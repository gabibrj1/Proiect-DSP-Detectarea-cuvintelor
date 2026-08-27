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

    size_t block_size = 320; // 20ms la 16kHz
    size_t max_blocks = audio.num_samples / block_size;
    BlockResult *results = malloc(max_blocks * sizeof(BlockResult));

    size_t num_blocks = compute_block_energy(audio.samples, audio.num_samples,
                                              block_size, results);

    // prag simplu, ales empiric pentru semnalul de test
    double threshold = 1000000.0;
    apply_threshold(results, num_blocks, threshold);

    printf("Total blocuri: %zu (block_size=%zu, ~%.1f ms/bloc)\n",
           num_blocks, block_size, 1000.0 * block_size / audio.sample_rate);

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