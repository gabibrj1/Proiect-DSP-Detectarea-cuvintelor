#include "energy_detector.h"

size_t compute_block_energy(const int16_t *samples, size_t num_samples,
                             size_t block_size, BlockResult *results) {
    size_t num_blocks = num_samples / block_size;

    for (size_t b = 0; b < num_blocks; b++) {
        double sum_sq = 0.0;
        size_t offset = b * block_size;

        for (size_t i = 0; i < block_size; i++) {
            double s = (double)samples[offset + i];
            sum_sq += s * s;
        }

        results[b].block_index = b;
        results[b].energy = sum_sq / block_size; // energie medie per esantion
        results[b].is_voice = 0; // se seteaza in apply_threshold
    }

    return num_blocks;
}

void apply_threshold(BlockResult *results, size_t num_blocks, double threshold) {
    for (size_t b = 0; b < num_blocks; b++) {
        results[b].is_voice = (results[b].energy > threshold) ? 1 : 0;
    }
}