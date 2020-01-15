/*
 * Copyright (C) 2019 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup tests
 * @{
 *
 * @file
 * @brief       puf_sram_node
 *
 * @author      Rene Herthel <rene.herthel@haw-hamburg.de>
 *
 * @}
 */

#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "shell.h"
#include "random.h"
#include "ecc/golay2412.h"
#include "ecc/repetition.h"
#include "puf_sram.h"
#include "periph/eeprom.h"

static inline void _print_buf(uint8_t *buf, size_t len, char *title)
{
    printf("%s\n", title);
    for (unsigned i = 0; i < len; i++) {
        printf("0x%02x ", buf[i]);
    }
    printf("\n- - - - - - - - - - - - - - - -\n\n");
}

static inline void _enrollment(void)
{
    static uint8_t helper[PUF_SRAM_HELPER_LEN] = {0};
    static uint8_t golay[PUF_SRAM_CODEOFFSET_LEN] = {0};
    static uint8_t repetition[PUF_SRAM_GOLAY_LEN] = {0};
    /* XXX: Fixed values for debug purposes. Will change later. */
    static uint8_t bytes[PUF_SRAM_CODEOFFSET_LEN] = {1, 1, 1, 1, 1, 1};

    puts("_enrollment() - start");

    /* TODO: Add support for multiple iterations.
          - Sum up in binary representation (if needed)
          - Binarize maximum to likelihood
          - Build the enr_bytes together again
          NOTE: The very first init of puf_sram_seed is the first iteration.
          Just hold it simple for the moment.
          dist/tools/puf-sram/puf_sram.py:56 */
    uint8_t *mle_response = (uint8_t *)puf_sram_seed;

    /* Write random bytes in range(0, 255) into bytes.
       NOTE: seed generated on sys/random/random.c:49 with
       random_init(puf_sram_seed).
       XXX: Fixed values are used ATM. */
    //random_bytes(bytes, PUF_SRAM_CODEOFFSET_LEN);

    golay2412_encode(PUF_SRAM_CODEOFFSET_LEN, bytes, golay);

    repetition_encode(PUF_SRAM_CODEOFFSET_LEN, golay, repetition);

    for(unsigned i = 0; i < PUF_SRAM_CODEOFFSET_LEN; i++) {
        helper[i] = repetition[i] ^ mle_response[i];
    }

    /* Save the generated helper data to non-volatile memory.
       NOTE: For the moment, it is restricted to EEPROM. */
    eeprom_write(PUF_SRAM_HELPER_EEPROM_START, helper, PUF_SRAM_HELPER_LEN);

    puts("_enrollment() - done");
}

static inline void _reconstruction(void)
{
    puts("_reconstruction() - start");
    /* Key is generated in puf_sram.c, during power cycle. */
    // TODO:
    // puf_sram_delete_secret();
    printf(" 0x");
    for (unsigned i=0;i<sizeof(puf_sram_id);i++){
        printf("%x", puf_sram_id[i]);
    }
    printf("\n");
    puf_sram_delete_secret();
    puts("_reconstruction() - done");
}

int main(void)
{
/*
#ifdef PUF_SRAM_GEN_HELPER
    _enrollment();
#else
#ifdef MODULE_PUF_SRAM_SECRET
    _reconstruction();
#endif
#endif
*/
    puts("\nRIOT/tests/puf-sram-node/");
    _enrollment();
    _reconstruction();
    return 0;
}