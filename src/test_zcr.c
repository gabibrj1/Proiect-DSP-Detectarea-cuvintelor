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

    compute_block_energy(audio.samples, audio.num_samples, block_size, results);
    compute_block_zcr(audio.samples, audio.num_samples, block_size, results);

    printf("bloc | timp(s) | energie      | zcr\n");
    printf("-----|---------|--------------|------\n");
    // afisam doar cateva blocuri reprezentative, nu toate 150
    for (size_t b = 0; b < max_blocks; b += 10) {
        double t = (double)(b * block_size) / audio.sample_rate;
        printf("%4zu | %7.3f | %12.2f | %.4f\n", b, t, results[b].energy, results[b].zcr);
    }

    free(results);
    wav_free(&audio);
    return 0;
}