/*
 * Copyright (C) 2018 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup     sys_puf_sram SRAM PUF
 * @ingroup      sys
 * @brief        SRAM based physically unclonable function (PUF)
 * @experimental This API is experimental and in an early state - expect
 *               changes!
 * @warning      The SRAM based seed mechanism it not cryptographically secure in its current
                 state.
 *
 * # About
 *
 * Transistor variations of SRAM memory cells lead to different states
 * after device power-on. The startup state of multiple memory
 * blocks form a device-unique pattern plus additional noise ("weak PUF").
 * The noise is used to generate random numbers for PRNG seeding.
 *
 * # Preliminaries
 *
 * High entropy numbers can only be generated when the device starts from power-off (including
 * low-power modes that turn of the RAM partly)
 * and before the memory has been used. That's why the SRAM PUF procedure is implemented
 * even before kernel initialization.
 * Memory properties are hardware specific and can depend on environmental conditions.
 * Thus, they should be evaluated for each individual deployment. A basic
 * testing tool is provided in /RIOT/tests/puf_sram.
 *
 * # Soft-reset detection
 *
 * In order to detect a software reboot without preceding power-off phase, a soft-reset
 * detection mechanism writes a marker memory @p PUF_SRAM_MARKER into SRAM. If the marker
 * is still present after a restart, a soft-reset is expected and the PUF procedure
 * is skipped.
 *
 * # Random Seed Generation
 *
 * Uninitialized memory pattern are compressed by the lightweight DEK hash function
 * to generate a high entropy 32-bit integer which can be used to seed a PRNG. This hash
 * function is not cryptographically secure and as such, adversaries might be able to
 * track parts of the initial SRAM response by analyzing PRNG sequences.
 *
 * @{
 * @file
 *
 * @author      Peter Kietzmann <peter.kietzmann@haw-hamburg.de>
 */
#ifndef PUF_SRAM_H
#define PUF_SRAM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include "thread.h"
#include "hashes.h"
#include "hashes/sha1.h"
#include "crypto/helper.h"

/**
 * @brief SRAM length considered for seed generation
 */
#ifndef PUF_SRAM_SEED_RAM_LEN
#define PUF_SRAM_SEED_RAM_LEN        (2048 / sizeof(uint32_t))
#endif

/**
 * @brief length of the random code offset for secret generation
          during enrollment. This value determines the helper data
          length and thus, it affects the PUF length. The PUF lenght
          itself affects the secrecy of the generated ID.
 */
#ifndef PUF_SRAM_SECRET_LEN
#define PUF_SRAM_SECRET_LEN          (6)
#endif

/**
 * @brief length of the encoded secret with golay(24, 12)
 */
#define PUF_SRAM_GOLAY_LEN           (2 * PUF_SRAM_SECRET_LEN)

/**
 * @brief Number of repetitions for the repetition encoder
 *        definded in Makefile.dep to configure the repetition
 *        coder during compile time
 */
#define PUF_SRAM_REP_COUNT           (ECC_REPETITION_COUNT)

/**
 * @brief length of the double encoded secret with
          repetition(PUF_SRAM_REP_COUNT, 1, PUF_SRAM_REP_COUNT)
 */
#define PUF_SRAM_HELPER_LEN          (PUF_SRAM_REP_COUNT * PUF_SRAM_GOLAY_LEN)

/**
 * @brief start position in non-volatile eeprom memory on which the
          public helper data set was stored during enrollment
 */
#define PUF_SRAM_HELPER_EEPROM_START (0)

/**
 * @brief offset in SRAM memory after a secret was generated via
          @p puf_sram_generate_secret
 */
#ifdef MODULE_PUF_SRAM_SECRET
#define PUF_SRAM_SEED_OFFSET         (PUF_SRAM_HELPER_EEPROM_START)
#else
#define PUF_SRAM_SEED_OFFSET         (0)
#endif

/**
 * @brief SRAM marker to detect reboot without power-off
 *
 * Source: https://www.random.org/bytes/
 */
#define PUF_SRAM_MARKER              (0xad3021ff)


/**
 * @brief Global seed variable, allocated in puf_sram.c
 */
extern uint32_t puf_sram_seed;

/**
 * @brief Global seed state, allocated in puf_sram.c
 *        0 means seed was generated from SRAM pattern,
          1 means missing power cycle detected,
          2 means power cycle detected. The state will most likely
            be overwritten with 0 in the next steps
 */
extern uint32_t puf_sram_state;

#if defined(MODULE_PUF_SRAM_SECRET) || defined(DOXYGEN)
 /**
 * @brief Global secret ID, allocated in puf_sram.c
 */
extern uint8_t puf_sram_id[SHA1_DIGEST_LENGTH];
#endif

/**
 * @brief Counter variable allocated in puf_sram.c. It is incremented
          during each soft reset when no new PUF measurement was taken
          and it gets reset to zero after a power cycle was detected.
 */
extern uint32_t puf_sram_softreset_cnt;

/**
 * @brief checks source of reboot by @p puf_sram_softreset and conditionally
 *        calls @p puf_sram_generate. If program is compiled with PUF_SRAM_GEN_HELPER
 *        this function copies the measured PUF response to the end of the RAM for
 *        helper data generation which is needed for reliable secret generation.
 *
 * @param[in] ram pointer to beginning of considered SRAM memory
 * @param[in] ram2 pointer to end of SRAM memory
 * @param[in] len length of the memroy to consider
 *
 */
void puf_sram_init(const uint8_t *ram, const uint8_t *ram2, size_t len);

/**
 * @brief builds seed from @p PUF_SRAM_SEED_RAM_LEN bytes uninitialized SRAM,
 *        and writes it to the global variable @p puf_sram_seed
 *
 * @param[in] ram pointer to SRAM memory
 * @param[in] len length of the memory to consider
 */
void puf_sram_generate_seed(const uint8_t *ram, size_t len);

#if defined(MODULE_PUF_SRAM_SECRET) || defined(DOXYGEN)
/**
 * @brief builds secret from @p PUF_SRAM_HELPER_LEN bytes uninitialized SRAM.
 *        This method utilizes a helper data set from non-volatile memory which
          needs to be generated for each device during enrollment
 *
 * @param[in] ram pointer to SRAM memory
 */
void puf_sram_generate_secret(const uint8_t *ram);

/**
 * @brief This function overwrites the PUF based secret variable @p puf_sram_id. It should
          be called as soon as possible afer the secret was used.
 */
static inline void puf_sram_delete_secret(void)
{
    /* TODO: make sure this isn't optimized out by the compiler */
    crypto_secure_wipe(puf_sram_id,sizeof(puf_sram_id));
}

#endif /* defined(MODULE_PUF_SRAM_SECRET) || defined(DOXYGEN) */

/**
 * @brief checks for a memory marker to determine whether memory contains old data.
          Otherwise it assumes a reboot from power down mode
 *
 * @return    0 when reset with power cycle was detected
 * @return    1 when reset without power cycle was detected
 */
bool puf_sram_softreset(void);


#ifdef __cplusplus
}
#endif
/** @} */
#endif /* PUF_SRAM_H */
