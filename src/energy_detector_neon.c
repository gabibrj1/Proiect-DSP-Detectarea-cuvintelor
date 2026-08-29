#include "energy_detector.h"
#include <arm_neon.h>

/*
 * Versiunea NEON a calculului de energie pe blocuri.
 *
 * Ideea de baza: pentru fiecare bloc de esantioane audio, calculam
 * suma patratelor esantioanelor (energia), la fel ca in versiunea
 * scalara, dar procesand cate 8 esantioane simultan folosind
 * registrele vectoriale NEON de 128 de biti.
 *
 * Un registru NEON de 128 biti poate tine 8 valori int16_t deodata,
 * deci in loc sa parcurgem esantioanele unul cate unul (ca in
 * versiunea scalara), le procesam in grupuri de 8.
 */
size_t compute_block_energy_neon(const int16_t *samples, size_t num_samples,
                                  size_t block_size, BlockResult *results) {
    size_t num_blocks = num_samples / block_size;

    for (size_t b = 0; b < num_blocks; b++) {
        const int16_t *block = samples + b * block_size;

        /*
         * Acumulatorul pentru suma finala trebuie sa fie pe 64 de biti.
         *
         * De ce: un esantion audio pe 16 biti poate avea valoarea
         * maxima ~32767. Ridicat la patrat, ajunge la ~1.07 miliarde,
         * ceea ce deja e aproape de limita unui int32 (~2.1 miliarde).
         * Daca am acumula sumele astea pe 32 de biti de-a lungul unui
         * bloc intreg (de exemplu 320 de esantioane), am depasi rapid
         * limita si am obtine overflow -> valori negative aberante.
         *
         * De aceea folosim int64x2_t ca acumulator principal, si
         * facem "widening" (extindere) la 64 de biti IMEDIAT dupa
         * fiecare grup de 8 esantioane procesate, nu doar la finalul
         * buclei. Asa acumulatorul mare nu are cum sa faca overflow
         * pentru dimensiuni de bloc rezonabile.
         */
        int64x2_t acc64 = vdupq_n_s64(0);

        size_t i = 0;

        // Procesam cate 8 esantioane 16-bit deodata.
        // Ne oprim cand nu mai avem 8 esantioane complete de procesat
        // (restul, daca exista, se face scalar mai jos).
        for (; i + 8 <= block_size; i += 8) {

            // Incarcam 8 esantioane consecutive intr-un registru NEON
            // de 128 de biti (8 x int16_t = 128 biti).
            int16x8_t v = vld1q_s16(&block[i]);

            /*
             * Ridicam la patrat fiecare esantion. Folosim vmull_s16,
             * care face inmultire cu "widening": rezultatul e pe 32
             * de biti in loc de 16, ca sa nu facem overflow la
             * inmultire (16 biti x 16 biti poate depasi 16 biti).
             *
             * Problema: vmull_s16 lucreaza doar pe jumatate de
             * registru (4 elemente), deci trebuie sa impartim
             * cele 8 elemente in doua jumatati: low (primele 4)
             * si high (ultimele 4).
             */
            int32x4_t lo = vmull_s16(vget_low_s16(v), vget_low_s16(v));   // patratele primelor 4 esantioane
            int32x4_t hi = vmull_s16(vget_high_s16(v), vget_high_s16(v)); // patratele ultimelor 4 esantioane

            // Adunam cele doua jumatati -> 4 valori pe 32 de biti.
            // Aici inca suntem in siguranta pe 32 biti, pentru ca
            // adunam doar 2 patrate (max ~2.1 miliarde), nu inca
            // acumulat pe mai multe iteratii.
            int32x4_t sum32 = vaddq_s32(lo, hi);

            /*
             * Acum e momentul critic: extindem imediat la 64 de biti
             * folosind vpaddlq_s32 ("pairwise add long") - aceasta
             * instructiune ia cele 4 valori pe 32 biti, le aduna in
             * perechi (elem0+elem1, elem2+elem3) si pune rezultatul
             * in 2 valori pe 64 de biti. Asa nu se pierde precizie
             * si nu se face overflow.
             */
            int64x2_t widened = vpaddlq_s32(sum32);

            // Adunam in acumulatorul mare, pe 64 de biti - aici putem
            // acumula in siguranta de-a lungul a mii de iteratii.
            acc64 = vaddq_s64(acc64, widened);
        }

        // Reducere finala: acc64 contine 2 valori pe 64 de biti (lane 0 si lane 1).
        // Le extragem si le adunam intr-un singur numar scalar.
        int64_t sum64 = vgetq_lane_s64(acc64, 0) + vgetq_lane_s64(acc64, 1);

        // Pornim suma finala de la ce am acumulat pe NEON.
        double sum_sq = (double)sum64;

        // Daca block_size nu e multiplu de 8, procesam "coada" ramasa
        // (ultimele 1-7 esantioane) scalar, normal, fara SIMD.
        for (; i < block_size; i++) {
            double s = (double)block[i];
            sum_sq += s * s;
        }

        results[b].block_index = b;
        results[b].energy = sum_sq / block_size; // energie medie per esantion, la fel ca la scalar
        results[b].is_voice = 0; // se seteaza separat, in apply_threshold
    }

    return num_blocks;
}