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

/* Buffer used in the enrollment. */
static uint8_t enr_helper[PUF_SRAM_HELPER_LEN] = {0};
static uint8_t enr_bytes[PUF_SRAM_CODEOFFSET_LEN] = {0};
static uint8_t enr_gol_enc[PUF_SRAM_CODEOFFSET_LEN] = {0};
static uint8_t enr_rep_enc[PUF_SRAM_GOLAY_LEN] = {0};

static inline void _print_buf(uint8_t *buf, size_t len, char *title)
{
    printf("%s\n", title);
    for (unsigned i = 0; i < len; i++) {
        printf("0x%02x ", buf[i]);
    }
    printf("\n- - - - - - - - - - - - - - - -\n\n");
}

int main(void)
{
    puts("\nRIOT/tests/puf-sram-node/");

    /* Defined during enrollment; Whenever helper data should be generated.
       The following generates helper data and write it into the EEPROM. */
#ifdef PUF_SRAM_GEN_HELPER

    /* TODO: Add support for multiple iterations.
          - Sum up in binary representation (if needed)
          - Binarize maximum to likelihood
          - Build the enr_bytes together again
          NOTE: The very first init of puf_sram_seed is the first iteration.
          Just hold it simple for the moment.
          dist/tools/puf-sram/puf_sram.py:56 */
    uint8_t *enr_puf_ref = (uint8_t *)puf_sram_seed;

    puts("PUF_SRAM_GEN_HELPER");
    _print_buf(enr_puf_ref, PUF_SRAM_HELPER_LEN, "enr_puf_ref:");
    /*
    puts("\nPUF measurement (puf_sram_seed):\n");
    for (unsigned i = 0; i < PUF_SRAM_HELPER_LEN; i++) {
        printf("0x%02x ", enr_puf_ref[i]);
    }
    puts("\n\n");
    */

    /* Write random bytes in range(0, 255) into enr_bytes.
       NOTE: seed generated on sys/random/random.c:49 with
       random_init(puf_sram_seed). */
    random_bytes(enr_bytes, PUF_SRAM_CODEOFFSET_LEN);
    _print_buf(enr_bytes, PUF_SRAM_CODEOFFSET_LEN, "enr_bytes:");

    /* The golay encoder takes the random sample of enr_bytes. */
    golay2412_encode(PUF_SRAM_CODEOFFSET_LEN, enr_bytes, enr_gol_enc);
    _print_buf(enr_gol_enc, PUF_SRAM_CODEOFFSET_LEN, "enr_gol_enc");

    /* The repetition encoder takes the encoded golay data. */
    repetition_encode(PUF_SRAM_CODEOFFSET_LEN, enr_gol_enc, enr_rep_enc);
    _print_buf(enr_rep_enc, PUF_SRAM_CODEOFFSET_LEN, "enr_rep_enc");

    /* XOR the result of the calc. repetition with the calc. MLE-response. */
    for(unsigned i = 0; i < PUF_SRAM_CODEOFFSET_LEN; i++) {
        enr_helper[i] = enr_rep_enc[i] ^ enr_puf_ref[i];
    }
    _print_buf(enr_helper, PUF_SRAM_CODEOFFSET_LEN, "enr_helper");

    /* Save the generated helper data to non-volatile memory.
       NOTE: For the moment, it is restricted to EEPROM. */
    eeprom_write(PUF_SRAM_HELPER_EEPROM_START, enr_helper, PUF_SRAM_HELPER_LEN);

    puts("Generating helper data is done.");
#else
    /* Defined during Reconstruction. Whenever a secret key should be generated. */
#ifdef MODULE_PUF_SRAM_SECRET
    puts("MODULE_PUF_SRAM_SECRET");
    // TODO:
    /*
        Key is generated in puf_sram.c, during power cycle.
     */
    // puf_sram_delete_secret();
    puts("Generating secret is done. (Does nothing yet)");
#endif /* MODULE_PUF_SRAM_SECRET */

#endif /* PUF_SRAM_GEN_HELPER */
    return 0;
}
