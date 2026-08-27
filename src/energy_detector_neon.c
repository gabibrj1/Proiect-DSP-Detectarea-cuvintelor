#include "energy_detector.h"
#include <arm_neon.h>

size_t compute_block_energy_neon(const int16_t *samples, size_t num_samples,
                                  size_t block_size, BlockResult *results) {
    size_t num_blocks = num_samples / block_size;

    for (size_t b = 0; b < num_blocks; b++) {
        const int16_t *block = samples + b * block_size;

        // Acumulator pe 4 lane-uri de 32-bit (evitam overflow fata de 16-bit)
        int32x4_t acc = vdupq_n_s32(0);

        size_t i = 0;
        // Procesam cate 8 esantioane 16-bit deodata (un registru NEON = 128 biti = 8x int16)
        for (; i + 8 <= block_size; i += 8) {
            int16x8_t v = vld1q_s16(&block[i]);        // incarca 8 esantioane
            int32x4_t lo = vmull_s16(vget_low_s16(v), vget_low_s16(v));   // patrat primele 4
            int32x4_t hi = vmull_s16(vget_high_s16(v), vget_high_s16(v)); // patrat ultimele 4
            acc = vaddq_s32(acc, lo);
            acc = vaddq_s32(acc, hi);
        }

        // Reducere orizontala: adunam cele 4 valori din acc intr-un singur numar
        int64x2_t acc64 = vpaddlq_s32(acc);           // extinde+aduna perechi -> 2x int64
        int64_t sum64 = vgetq_lane_s64(acc64, 0) + vgetq_lane_s64(acc64, 1);

        // Restul de esantioane (daca block_size nu e multiplu de 8) - procesate scalar
        double sum_sq = (double)sum64;
        for (; i < block_size; i++) {
            double s = (double)block[i];
            sum_sq += s * s;
        }

        results[b].block_index = b;
        results[b].energy = sum_sq / block_size;
        results[b].is_voice = 0;
    }

    return num_blocks;
}