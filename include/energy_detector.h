#ifndef ENERGY_DETECTOR_H
#define ENERGY_DETECTOR_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    size_t block_index;
    double energy;      // suma patratelor esantioanelor din bloc, normalizata
    int is_voice;        // 1 = voce/activ, 0 = tacere
    double zcr;        // zero-crossing rate normalizat (0.0 - 1.0)
} BlockResult;

// Calculeaza energia pe blocuri de dimensiune fixa (block_size esantioane)
// Numarul de blocuri = num_samples / block_size (ultimul bloc partial e ignorat)
// results trebuie sa aiba spatiu pentru cel putin (num_samples / block_size) elemente
// Returneaza numarul de blocuri procesate
size_t compute_block_energy(const int16_t *samples, size_t num_samples,
                             size_t block_size, BlockResult *results);

// Aplica un prag fix pe energie ca sa marcheze is_voice in fiecare bloc
void apply_threshold(BlockResult *results, size_t num_blocks, double threshold);

// Versiune NEON (ARM SIMD) a compute_block_energy
size_t compute_block_energy_neon(const int16_t *samples, size_t num_samples,
                                  size_t block_size, BlockResult *results);

// Calculeaza zero-crossing rate (ZCR) pentru fiecare bloc
// ZCR = de cate ori semnul esantioanelor se schimba intre doi vecini consecutivi, normalizat la numarul de esantioane din bloc
// adaugam doar campul zcr peste ce exista deja

void compute_block_zcr(const int16_t *samples, size_t num_samples, size_t block_size, BlockResult *results);

// calculeaza un prag adaptiv de energie pe baza nivelului de zgomot de fond estimat din primele "noise_blocks" blocuri
// presupunem ca semnalul incepe cu tacere/zgomot ambiental
// factor controleaza cat de sus fata de zgomotul de fond trebuie sa fie energia ca sa fie considerata voce

double compute_adaptive_threshold(const BlockResult *results, size_t num_blocks, size_t noise_blocks, double factor);

// aplica decizia finala voce/tacere folosind prag adaptiv + hangover
// hangover_blocks = cate blocuri tinem starea VOCE dupa ce energia scade sub prag, ca sa nu taiem prematur sfarsitul cuvintelor

void apply_adaptive_threshold(BlockResult *results, size_t num_blocks, double threshold, size_t hangover_blocks);

#endif