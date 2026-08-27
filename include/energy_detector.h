#ifndef ENERGY_DETECTOR_H
#define ENERGY_DETECTOR_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    size_t block_index;
    double energy;      // suma patratelor esantioanelor din bloc, normalizata
    int is_voice;        // 1 = voce/activ, 0 = tacere
} BlockResult;

// Calculeaza energia pe blocuri de dimensiune fixa (block_size esantioane)
// Numarul de blocuri = num_samples / block_size (ultimul bloc partial e ignorat)
// results trebuie sa aiba spatiu pentru cel putin (num_samples / block_size) elemente
// Returneaza numarul de blocuri procesate
size_t compute_block_energy(const int16_t *samples, size_t num_samples,
                             size_t block_size, BlockResult *results);

// Aplica un prag fix pe energie ca sa marcheze is_voice in fiecare bloc
void apply_threshold(BlockResult *results, size_t num_blocks, double threshold);

#endif