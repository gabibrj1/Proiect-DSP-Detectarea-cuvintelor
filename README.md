# Word Detection with SIMD NEON on Raspberry Pi 4

Voice activity detector for audio files, with the core energy-computation kernel implemented
both as a scalar baseline and vectorized with ARM NEON intrinsics, to measure the real speedup
SIMD gives on a Raspberry Pi 4.

## Hardware and toolchain

- **Board:** Raspberry Pi 4 Model B, 2GB RAM, Broadcom BCM2711 SoC (4x Cortex-A72, ARMv8-A
  64-bit), NEON available natively (not optional on A72, unlike some 32-bit A-series cores)
- **OS on device:** Raspberry Pi OS Lite (64-bit)
- **Compiler:** gcc 14.2.0 (native on the board) / gcc 13.3.0 (WSL Ubuntu, for x86_64
  development — only the non-NEON code builds there)
- **Workflow:** editing in WSL (VS Code), final compilation/testing over SSH directly on the
  board, since `energy_detector_neon.c` needs a real ARM compiler

## Code layout

```
include/
  wav_reader.h          # WavAudio struct, wav_read(), wav_free()
  energy_detector.h      # BlockResult struct + all function prototypes
src/
  wav_reader.c            # RIFF/WAVE parser
  generate_test_wav.c      # generates a synthetic test signal (440Hz tone + background noise)
  energy_detector.c        # compute_block_energy() - scalar version
  energy_detector_neon.c   # compute_block_energy_neon() - SIMD version
  zcr_detector.c           # compute_block_zcr()
  adaptive_threshold.c     # compute_adaptive_threshold() + apply_adaptive_threshold()
  benchmark.c              # measures scalar vs NEON timing with clock_gettime()
  test_*.c                 # one test driver per module above
data/
  test_voice.wav           # own recording: "one, two, three, four" (spoken in Romanian)
```

## Processing pipeline

```
.wav file → wav_read() → int16_t* samples[]
                                  │
                    ┌─────────────┼─────────────┐
                    ▼                            ▼
        compute_block_energy()        compute_block_zcr()
        (or _neon())                         │
                    │                         │
                    └───────────┬─────────────┘
                                 ▼
                  compute_adaptive_threshold()
                  (threshold = noise_floor_avg × factor)
                                 │
                                 ▼
                  apply_adaptive_threshold()
                  (VOICE/SILENCE decision + hangover)
                                 │
                                 ▼
                  VOICE↔SILENCE transitions = detected words
```

## Data structures

```c
// wav_reader.h
typedef struct {
    uint32_t sample_rate;
    uint16_t num_channels;
    uint16_t bits_per_sample;
    uint32_t num_samples;   // per channel
    int16_t *samples;       // dynamically allocated (malloc), interleaved if stereo
} WavAudio;
```

`wav_read()` parses the RIFF chunks (`fmt `, `data`) manually, with no external dependency —
it only supports uncompressed PCM, 16 bits/sample (checked explicitly in code; any other
format or bit depth returns an error).

```c
// energy_detector.h
typedef struct {
    size_t block_index;
    double energy;      // sum of squared samples in the block, normalized
    int is_voice;        // 1 = voice/active, 0 = silence
    double zcr;          // normalized zero-crossing rate (0.0 - 1.0)
} BlockResult;
```

One `BlockResult` per block of samples (default 320 samples = 20ms at 16kHz). Every function
in the pipeline fills in a different field of the same `BlockResult` array.

## Energy kernel — scalar version

```c
size_t compute_block_energy(const int16_t *samples, size_t num_samples,
                             size_t block_size, BlockResult *results) {
    size_t num_blocks = num_samples / block_size;
    for (size_t b = 0; b < num_blocks; b++) {
        double sum_sq = 0.0;
        for (size_t i = 0; i < block_size; i++) {
            double s = (double)samples[b * block_size + i];
            sum_sq += s * s;
        }
        results[b].energy = sum_sq / block_size;
    }
    return num_blocks;
}
```

A sum of squares per block, normalized by block size (average energy per sample, not total
energy — this keeps the threshold independent of `block_size`).

## Energy kernel — NEON version

Same operation, but processing 8 `int16_t` samples at once (one 128-bit NEON register = 8 ×
16 bits). The non-trivial part is handling bit width correctly at every step to avoid
overflow:

```c
int64x2_t acc64 = vdupq_n_s64(0);   // accumulator IN 64 BITS

for (; i + 8 <= block_size; i += 8) {
    int16x8_t v = vld1q_s16(&block[i]);                          // load 8 samples

    int32x4_t lo = vmull_s16(vget_low_s16(v), vget_low_s16(v));  // square, widening to 32 bits
    int32x4_t hi = vmull_s16(vget_high_s16(v), vget_high_s16(v));
    int32x4_t sum32 = vaddq_s32(lo, hi);                          // 4 values, still safe at 32 bits

    int64x2_t widened = vpaddlq_s32(sum32);                       // pairwise-add + widen to 64 bits
    acc64 = vaddq_s64(acc64, widened);                            // final accumulation, 64 bits
}

int64_t sum64 = vgetq_lane_s64(acc64, 0) + vgetq_lane_s64(acc64, 1);
```

**Why bit width matters here, concretely:** a 16-bit sample can reach ±32767. Squared, that's
~1.07 billion — already close to the limit of an `int32` (~2.1 billion). If the main
accumulator stayed at 32 bits across an entire block (320 samples), the sum would quickly
overflow and produce garbage negative values (a silent overflow, with no compile-time or
run-time error — just wrong results). This was exactly the bug hit during development: the
first version of the code only widened to 64 bits at the very end of the loop, not after each
group of 8 samples, and produced negative energies on high-signal blocks. The fix was moving
the widening step (`vpaddlq_s32`) **inside** the loop, right after each iteration.

`vmull_s16` only operates on half a register (4 out of 8 elements), hence the explicit split
into `vget_low_s16`/`vget_high_s16` — a direct consequence of NEON's fixed register width, not
a stylistic choice.

The remaining tail (if `block_size` isn't a multiple of 8) is handled with plain scalar code
after the vectorized loop.

## Correctness validation

`test_compare_scalar_neon.c` runs both versions on the same signal and compares energy
block by block:

```c
double diff = fabs(results_scalar[b].energy - results_neon[b].energy);
```

After the overflow fix described above, the maximum absolute difference across the whole test
signal is **0.0** — bit-identical results, not just "close enough".

## Adaptive threshold and hangover

The threshold isn't a manually-picked constant — it's computed from the signal itself:

```c
double compute_adaptive_threshold(const BlockResult *results, size_t num_blocks,
                                   size_t noise_blocks, double factor) {
    double sum = 0.0;
    for (size_t b = 0; b < noise_blocks; b++) sum += results[b].energy;
    double noise_floor = sum / noise_blocks;
    return noise_floor * factor;
}
```

It assumes the signal starts with background noise/silence (the first `noise_blocks` blocks),
and the threshold is that average energy multiplied by a safety `factor`. In testing,
`noise_blocks=10` (200ms) and `factor=4.0` turned out to be a good compromise: high enough to
reject background noise, low enough not to miss weak consonants.

The final decision adds hangover — once in the VOICE state, it stays there for
`hangover_blocks` more blocks even if energy drops below the threshold, before actually
switching to SILENCE:

```c
if (results[b].energy > threshold) {
    results[b].is_voice = 1;
    hangover_counter = hangover_blocks;
} else if (hangover_counter > 0) {
    results[b].is_voice = 1;   // still in hangover, count as VOICE
    hangover_counter--;
} else {
    results[b].is_voice = 0;   // hangover exhausted, actually SILENCE
}
```

Without hangover, a word ending in a weak consonant (low energy) risked being cut off before
its real end. With `hangover_blocks=15` (300ms at block_size 320/16kHz), a test word that
previously split into two separate segments merged correctly into one interval.

## Building

```bash
mkdir -p build

# synthetic test signal
gcc -o build/generate_test_wav src/generate_test_wav.c -lm
./build/generate_test_wav

# non-NEON modules — build anywhere (x86_64 or ARM)
gcc -Iinclude -O2 -Wall -o build/test_wav_reader src/test_wav_reader.c src/wav_reader.c
gcc -Iinclude -O2 -Wall -o build/test_energy_detector src/test_energy_detector.c src/wav_reader.c src/energy_detector.c
gcc -Iinclude -O2 -Wall -o build/test_zcr src/test_zcr.c src/wav_reader.c src/energy_detector.c
gcc -Iinclude -O2 -Wall -o build/test_adaptive src/test_adaptive.c src/wav_reader.c src/energy_detector.c src/adaptive_threshold.c

# NEON modules — need an ARM compiler (native on the board, or an aarch64 cross-compiler)
gcc -Iinclude -O2 -march=armv8-a+simd -Wall -o build/test_compare src/test_compare_scalar_neon.c src/wav_reader.c src/energy_detector.c src/energy_detector_neon.c -lm
gcc -Iinclude -O2 -march=armv8-a+simd -Wall -o build/benchmark src/benchmark.c src/wav_reader.c src/energy_detector.c src/energy_detector_neon.c
```

`-march=armv8-a+simd` is only strictly necessary when using a cross-compiler; natively on the
board gcc detects NEON automatically via `-march=native`, but the explicit flag makes the
build command portable regardless of environment.

### Running

```bash
./build/test_adaptive data/test_voice.wav      # prints VOICE/SILENCE transitions
./build/benchmark data/test_voice.wav 100      # 100 repeats, for stable measurements
```

`benchmark.c` runs each variant (scalar/NEON) `repeats` times in a row and measures total time
with `clock_gettime(CLOCK_MONOTONIC, ...)` — a single run on a few-second file is too fast (below
the useful clock resolution, dominated by system noise) to give a trustworthy number.

## Measured results

**NEON vs. scalar speedup** (ratio of scalar time / NEON time, measured on the board):

| Signal | Duration | Blocks | Speedup |
|---|---|---|---|
| Synthetic tone (440Hz + background noise) | 3.0 s | 150 | 7.51x |
| Real voice ("one, two, three, four") | 10.0 s | 500 | 7.09x |

The theoretical ceiling is 8x (one NEON register processes 8 `int16_t` samples per
instruction) — the measured result sits at 88-94% of that theoretical max, the gap coming from
the final-reduction overhead (`vpaddlq`/`vgetq_lane`) and the scalar tail for leftover samples.

**Detection on real voice:** with `factor=4.0` and `hangover_blocks=15`, all 4 words in
`test_voice.wav` are detected correctly as 4 distinct VOICE intervals, with no false positives
from the background noise at the start of the recording (which stays below the automatically
computed adaptive threshold).