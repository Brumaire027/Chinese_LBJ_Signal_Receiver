//
// Multimon-NG POCSAG BCH(31,21) Correction Functions.
// From https://github.com/EliasOenal/multimon-ng/blob/master/pocsag.c
// Migrated by FLN1021 on 2023/9/1.
//

#ifndef PAGER_RECEIVE_BCH3121A_H
#define PAGER_RECEIVE_BCH3121A_H

#include "string.h"

/*
 * the code used by POCSAG is a (n=31,k=21) BCH Code with dmin=5,
 * thus it could correct two bit errors in a 31-Bit codeword.
 * It is a systematic code.
 * The generator polynomial is:
 *   g(x) = x^10+x^9+x^8+x^6+x^5+x^3+1
 * The parity check polynomial is:
 *   h(x) = x^21+x^20+x^18+x^16+x^14+x^13+x^12+x^11+x^8+x^5+x^3+1
 * g(x) * h(x) = x^n+1
 */
#define BCH_POLY 03551 /* octal */
#define BCH_N    31
#define BCH_K    21


#endif //PAGER_RECEIVE_BCH3121A_H
