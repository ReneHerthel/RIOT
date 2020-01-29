/*
 * Copyright (C) 2018 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     sys_puf_sram
 *
 * @{
 * @file
 *
 * @author      Peter Kietzmann <peter.kietzmann@haw-hamburg.de>
 *
 * @}
 */
#include "puf_sram.h"
#include "ecc/golay2412.h"
#include "ecc/repetition.h"
#include "hashes/sha1.h"

#ifdef USE_EEPROM
#include "periph/eeprom.h"
#else

#ifdef USE_N25Q128
#include "board.h"
#include "n25q128.h"
#endif

#endif

/* TODO: REMOVE ME */
PUF_SRAM_ATTRIBUTES uint8_t codeoffset_debug[6];
PUF_SRAM_ATTRIBUTES uint8_t ram_debug[PUF_SRAM_HELPER_LEN];
PUF_SRAM_ATTRIBUTES uint8_t helper_debug[PUF_SRAM_HELPER_LEN];

/* Allocation of the PUF seed variable */
PUF_SRAM_ATTRIBUTES uint32_t puf_sram_seed;

/* Allocation of the PUF seed state */
PUF_SRAM_ATTRIBUTES uint32_t puf_sram_state;

/* Allocation of the PUF soft reset conter*/
PUF_SRAM_ATTRIBUTES uint32_t puf_sram_softreset_cnt;

/* Allocation of the memory marker */
PUF_SRAM_ATTRIBUTES uint32_t puf_sram_marker;

#ifdef MODULE_PUF_SRAM_SECRET
/* Allocation of the secret ID memory. Set to 0 as soon as posisble! */
PUF_SRAM_ATTRIBUTES uint8_t puf_sram_id[SHA1_DIGEST_LENGTH];
#endif

void puf_sram_init(const uint8_t *ram, const uint8_t *ram2, size_t len)
{
    /* Read from eeprom and save in 'global' array for debugging. */
    /*
    static uint8_t helper[PUF_SRAM_HELPER_LEN];
    eeprom_read(PUF_SRAM_HELPER_EEPROM_START, helper, PUF_SRAM_HELPER_LEN);
    for (unsigned i = 0; i < PUF_SRAM_HELPER_LEN; i++) {
        helper_debug[i] = helper[i];
    }
    */

    /* Save memory pattern to end of RAM. */
    uint32_t *dst = (uint32_t *)(ram2-PUF_SRAM_HELPER_LEN);
    puf_sram_seed = (uint32_t)dst;
    for (unsigned i = 0; i < (PUF_SRAM_HELPER_LEN / sizeof(uint32_t)); i++){
        *(dst++) = ((uint32_t *)ram)[i];
    }

    /* Check for power cycle. */
    if (!puf_sram_softreset()) {
        puf_sram_generate_seed(ram + PUF_SRAM_SEED_OFFSET, len);
    }

/*
#ifdef PUF_SRAM_GEN_HELPER
    (void)len;
    // save memory pattern by copying to end of RAM
    uint32_t *dst = (uint32_t *)(ram2-PUF_SRAM_HELPER_LEN);
    // misuse seed variable to save location of copied puf response
    puf_sram_seed = (uint32_t)dst;
    for (unsigned i = 0; i < (PUF_SRAM_HELPER_LEN / sizeof(uint32_t)); i++){
        *(dst++) = ((uint32_t *)ram)[i];
    }
#else
    (void)ram2;
    // generates a new seed value if power cycle was detected
    if (!puf_sram_softreset()) {
        // TODO: revert this changes:
        static uint8_t helper[PUF_SRAM_HELPER_LEN];
        // get public helper data from non-volatile memory
        eeprom_read(PUF_SRAM_HELPER_EEPROM_START, helper, PUF_SRAM_HELPER_LEN);
        // TODO: Remove. Only for debug
        for (unsigned i = 0; i < PUF_SRAM_HELPER_LEN; i++) {
            helper_debug[i] = helper[i];
        }
#ifdef MODULE_PUF_SRAM_SECRET
        puf_sram_generate_secret(ram);
#endif
        puf_sram_generate_seed(ram + PUF_SRAM_SEED_OFFSET, len);
    }
#endif
*/
}

#ifdef MODULE_PUF_SRAM_SECRET
void puf_sram_generate_secret(const uint8_t *ram)
{
    static uint8_t helper[PUF_SRAM_HELPER_LEN];

    uint8_t fuzzy_io[sizeof(helper)];
    uint8_t rep_dec[PUF_SRAM_GOLAY_LEN];
    uint8_t golay_dec[PUF_SRAM_CODEOFFSET_LEN];


    /* get public helper data from non-volatile memory */
#ifdef USE_EEPROM
    eeprom_read(PUF_SRAM_HELPER_EEPROM_START, helper, PUF_SRAM_HELPER_LEN);
#else

#ifdef USE_N25Q128
    /* XXX: The device in the main has the same configuration. */
    static n25q128_dev_t n25q128;
    n25q128.conf.bus = EXTFLASH_SPI;
    n25q128.conf.mode = SPI_MODE_0;
    n25q128.conf.clk = SPI_CLK_100KHZ;
    n25q128.conf.cs = EXTFLASH_CS;
    n25q128.conf.write = EXTFLASH_WRITE;
    n25q128.conf.hold = EXTFLASH_HOLD;
    n25q128_read_data_bytes(&n25q128, 0, helper, PUF_SRAM_HELPER_LEN);
#endif

#endif

    // TODO: Remove. Only for debug
    for (unsigned i = 0; i < PUF_SRAM_HELPER_LEN; i++) {
        helper_debug[i] = helper[i];
    }

    // TODO: Remove. Only for debug.
    for (unsigned i = 0; i < PUF_SRAM_HELPER_LEN; i++) {
        ram_debug[i] = ram[i];
    }

    for(unsigned i=0; i<sizeof(helper); i++) {
        fuzzy_io[i] = helper[i] ^ ram[i];
    }

    /* correct the noisy fuzzy_io by decoding ECCs */
    repetition_decode(sizeof(rep_dec), &fuzzy_io[0], &rep_dec[0]);
    golay2412_decode(sizeof(golay_dec), &rep_dec[0], &golay_dec[0]);

    /* TODO: REMOVE ME */
    for (unsigned i = 0; i < 6; i++) {
        codeoffset_debug[i] = golay_dec[i];
    }

    /* encode again to generate a corrected PUF response */
    golay2412_encode(sizeof(golay_dec), &golay_dec[0], &rep_dec[0]);
    repetition_encode(sizeof(rep_dec), &rep_dec[0], &fuzzy_io[0]);

    for(unsigned i=0; i<sizeof(helper); i++) {
        fuzzy_io[i] ^= helper[i];
    }

    /* generate ID by hashing corrected PUF response */
    sha1_context ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, fuzzy_io, sizeof(fuzzy_io));
    sha1_final(&ctx, puf_sram_id);
}
#endif /* defined(MODULE_PUF_SRAM_SECRET) */

void puf_sram_generate_seed(const uint8_t *ram, size_t len)
{
    /* build hash from start-up pattern */
    puf_sram_seed = dek_hash(ram, len);
    /* write marker to a defined section for subsequent reset detection */
    puf_sram_marker = PUF_SRAM_MARKER;
    /* setting state to 0 means seed was generated from SRAM pattern */
    puf_sram_state = 0;
    /* reset counter of detected soft resets */
    puf_sram_softreset_cnt = 0;
}

bool puf_sram_softreset(void)
{
    if (puf_sram_marker != PUF_SRAM_MARKER) {
        puf_sram_state = 2;
        return 0;
    }
    puf_sram_state = 1;

    /* increment number of detected soft resets */
    puf_sram_softreset_cnt++;

    /* generate alterntive seed value */
    puf_sram_seed ^= puf_sram_softreset_cnt;
    puf_sram_seed = dek_hash((uint8_t *)&puf_sram_seed, sizeof(puf_sram_seed));
    return 1;
}
