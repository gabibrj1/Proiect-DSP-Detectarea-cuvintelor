#include "energy_detector.h"

// media energiei pe primele "nouise_blocks" blocuri = estimare a nivelului de zgomot de fond( presupunem ca semnaulul incepe cu tacere sau zgomot ambiental, nu direct cu voce)

double compute_adaptive_threshold(const BlockResult *results, size_t num_blocks,
                                   size_t noise_blocks, double factor) {
    // daca semnalul e mai scurt decat ce cerem, folosim tot ce avem
    if (noise_blocks > num_blocks) {
        noise_blocks = num_blocks;
    }
    if (noise_blocks == 0) {
        return 0.0; // nu avem date, prag 0 (totul e voce, degenerat)
    }

    double sum = 0.0;
    for (size_t b = 0; b < noise_blocks; b++) {
        sum += results[b].energy;
    }
    double noise_floor = sum / noise_blocks;

    // pragul e nivelul de zgomot inmultit cu un factor de siguranta -
    // vrem sa fim clar deasupra zgomotului, nu doar putin peste el
    return noise_floor * factor;
}

// aplica decizia voce/tacere cu hangover: odata ce am intrat in stare VOCE, ramanem in ea inca "hangover_blocks" blocuri dupa ce energia scade sub prag, inainte sa comutam efectiv pe TACERE
// asta evita taierea prematura a sunetelor slabe de la finalul
// cuvintelor (ex. consoane surde, terminatii care se sting treptat)
void apply_adaptive_threshold(BlockResult *results, size_t num_blocks,
                               double threshold, size_t hangover_blocks) {
    int hangover_counter = 0;

    for (size_t b = 0; b < num_blocks; b++) {
        if (results[b].energy > threshold) {
            // energie peste prag - clar voce, resetam contorul de hangover
            results[b].is_voice = 1;
            hangover_counter = hangover_blocks;
        } else if (hangover_counter > 0) {
            // energie sub prag, dar inca in perioada de hangover -
            // consideram tot voce, si numaram descrescator
            results[b].is_voice = 1;
            hangover_counter--;
        } else {
            // energie sub prag si hangover epuizat - efectiv tacere
            results[b].is_voice = 0;
        }
    }
}