#include "energy_detector.h"

// numaram schimburile de semn intre esantioane consecutive
// un "zero-crossing" apare cand semnul esantionului curent difera de semnul celui anterior

void compute_block_zcr(const int16_t *samples, size_t num_samples, size_t block_size, BlockResult *results) {
    size_t num_blocks = num_samples / block_size;

    for(size_t b = 0; b < num_blocks; b++) {
        const int16_t *block = samples + b * block_size;
        size_t crossings = 0;

        // pornim de la al doilea esantion, comparam mereu cu cel anterior
        for(size_t i = 1; i < block_size; i++) {
            // verificam daca semnele difera inmultind cele doua valori, daca produsul e negativ
            // inseamna ca una era pozitiva si cealalta negativa
            if ((block[i - 1] >= 0) != (block[i] >= 0)) {
                crossings++;
            }
        }   
        // normalizam la [0, 1] impartind la numarul maxim posibil de treceri prin zero
        // block_size - 1 comparatii facute
        results[b].zcr = (double)crossings / (block_size - 1);
    }
}