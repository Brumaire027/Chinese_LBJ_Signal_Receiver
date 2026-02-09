//
// Multimon-NG POCSAG BCH(31,21) Correction Functions.
// Migrated by FLN1021 on 2023/9/1.
//
#include "BCH3121A.h"

static inline unsigned char even_parity(uint32_t data) {
    unsigned int temp = data ^ (data >> 16);

    temp = temp ^ (temp >> 8);
    temp = temp ^ (temp >> 4);
    temp = temp ^ (temp >> 2);
    temp = temp ^ (temp >> 1);
    return temp & 1;
}


/* ---------------------------------------------------------------------- */

static unsigned int pocsag_syndrome(uint32_t data) {
    uint32_t shreg = data >> 1; /* throw away parity bit */
    uint32_t mask = 1L << (BCH_N - 1), coeff = BCH_POLY << (BCH_K - 1);
    int n = BCH_K;

    for (; n > 0; mask >>= 1, coeff >>= 1, n--)
        if (shreg & mask)
            shreg ^= coeff;
    if (even_parity(data))
        shreg |= (1 << (BCH_N - BCH_K));
//    verbprintf(9, "BCH syndrome: data: %08lx syn: %08lx\n", data, shreg);
    return shreg;
}

static uint32_t
transpose_n(int n, uint32_t *matrix) {
    uint32_t out = 0;
    int j;

    for (j = 0; j < 32; ++j) {
        if (matrix[j] & (1 << n))
            out |= (1 << j);
    }

    return out;
}

#define ONE 0xffffffff

static uint32_t *
transpose_clone(uint32_t src, uint32_t *out) {
    int i;
    if (!out)
        out = malloc(sizeof(uint32_t) * 32);

    for (i = 0; i < 32; ++i) {
        if (src & (1 << i))
            out[i] = ONE;
        else
            out[i] = 0;
    }

    return out;
}

static void
bitslice_syndrome(uint32_t *slices) {
    const int firstBit = BCH_N - 1;
    int i, n;
    uint32_t paritymask = slices[0];

    // do the parity and shift together
    for (i = 1; i < 32; ++i) {
        paritymask ^= slices[i];
        slices[i - 1] = slices[i];
    }
    slices[31] = 0;

    // BCH_POLY << (BCH_K - 1) is
    //                                                              20   21 22 23
    //  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, ONE, 0, 0, ONE,
    //  24 25   26  27  28   29   30   31
    //  0, ONE, ONE, 0, ONE, ONE, ONE, 0

    for (n = 0; n < BCH_K; ++n) {
        // one line here for every '1' bit in coeff (above)
        const int bit = firstBit - n;
        slices[20 - n] ^= slices[bit];
        slices[23 - n] ^= slices[bit];
        slices[25 - n] ^= slices[bit];
        slices[26 - n] ^= slices[bit];
        slices[28 - n] ^= slices[bit];
        slices[29 - n] ^= slices[bit];
        slices[30 - n] ^= slices[bit];
        slices[31 - n] ^= slices[bit];
    }

    // apply the parity mask we built up
    slices[BCH_N - BCH_K] |= paritymask;
}

/* ---------------------------------------------------------------------- */



// This might not be elegant, yet effective!
// Error correction via bruteforce ;)
//
// It's a pragmatic solution since this was much faster to implement
// than understanding the math to solve it while being as effective.
// Besides that the overhead is neglectable.
int pocsag_brute_repair(uint32_t *data, uint32_t *errors, uint32_t *err_corrected, int pocsag_error_correction) {
    if (pocsag_syndrome(*data)) {
//        rx->pocsag_total_error_count++;
        errors++;
//        verbprintf(6, "Error in syndrome detected!\n");
    } else {
        return 0;
    }

    if (pocsag_error_correction == 0) {
//        rx->pocsag_uncorrected_error_count++;
//        verbprintf(6, "Couldn't correct error!\n");
        return 1;
    }

    // check for single bit errors
    {
        int i, n, b1, b2;
        uint32_t res;
        uint32_t *xpose = 0, *in = 0;

        xpose = malloc(sizeof(uint32_t) * 32);
        in = malloc(sizeof(uint32_t) * 32);

        transpose_clone(*data, xpose);
        for (i = 0; i < 32; ++i)
            xpose[i] ^= (1 << i);

        bitslice_syndrome(xpose);

        res = 0;
        for (i = 0; i < 32; ++i)
            res |= xpose[i];
        res = ~res;

        if (res) {
            int n = 0;
            while (res) {
                ++n;
                res >>= 1;
            }
            --n;

            *data ^= (1 << n);
//            rx->pocsag_corrected_error_count++;
//            rx->pocsag_corrected_1bit_error_count++;
            err_corrected++;
            goto returnfree;
        }

        if (pocsag_error_correction == 1) {
//            rx->pocsag_uncorrected_error_count++;
//            verbprintf(6, "Couldn't correct error!\n");
            if (xpose)
                free(xpose);
            if (in)
                free(in);
            return 1;
        }

        //check for two bit errors
        n = 0;
        transpose_clone(*data, xpose);

        for (b1 = 0; b1 < 32; ++b1) {
            for (b2 = b1; b2 < 32; ++b2) {
                xpose[b1] ^= (1 << n);
                xpose[b2] ^= (1 << n);

                if (++n == 32) {
                    memcpy(in, xpose, sizeof(uint32_t) * 32);

                    bitslice_syndrome(xpose);

                    res = 0;
                    for (i = 0; i < 32; ++i)
                        res |= xpose[i];
                    res = ~res;

                    if (res) {
                        int n = 0;
                        while (res) {
                            ++n;
                            res >>= 1;
                        }
                        --n;

                        *data = transpose_n(n, in);
//                        rx->pocsag_corrected_error_count++;
//                        rx->pocsag_corrected_2bit_error_count++;
                        err_corrected++;
                        goto returnfree;
                    }

                    transpose_clone(*data, xpose);
                    n = 0;
                }
            }
        }

        if (n > 0) {
            memcpy(in, xpose, sizeof(uint32_t) * 32);

            bitslice_syndrome(xpose);

            res = 0;
            for (i = 0; i < 32; ++i)
                res |= xpose[i];
            res = ~res;

            if (res) {
                int n = 0;
                while (res) {
                    ++n;
                    res >>= 1;
                }
                --n;

                *data = transpose_n(n, in);
//                rx->pocsag_corrected_error_count++;
//                rx->pocsag_corrected_2bit_error_count++;
                err_corrected++;
                goto returnfree;
            }
        }

//        rx->pocsag_uncorrected_error_count++;
//        verbprintf(6, "Couldn't correct error!\n");
        if (xpose)
            free(xpose);
        if (in)
            free(in);
        return 1;

        returnfree:
        if (xpose)
            free(xpose);
        if (in)
            free(in);
        return 0;
    }
}