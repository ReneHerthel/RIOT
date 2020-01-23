/*
 * Copyright (C) 2018 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    sys_puf_sram SRAM PUF
 * @ingroup     sys
 * @brief       SRAM based physically unclonable function (PUF)
 * @{
 *
 * @experimental This API is experimental and in an early state - expect
 *               significant changes!
 *
 * # About
 *
 * Transistor variations of SRAM memory cells lead to different states
 * after device power-on. The startup state of multiple memory
 * blocks form a device-unique pattern plus additional noise ("weak PUF").
 * The noise is used to generate random numbers for PRNG seeding.
 * The identifying component is used to generate secret IDs.
 *
 *
 * # Preliminaries
 *
 * High entropy numbers can only be generated when the device starts from power-off
 * and before the memory has been used. That's why the SRAM PUF procedure is implemented
 * even before the actual OS initialization. That means threading, scheduling, peripherals,
 * drivers or networking modules are **not** available from there, as they were not
 * initialized. Not even clocks were configured which might lead to unexpected processing
 * times for the PUF procedure.
 *
 * Memory properties depend on the hardware itself as well as environmental parameters
 * and thus, they should be evaluated for each individual deployment. A basic
 * testing tool is provided in /RIOT/tests/puf_sram.
 *
 * ## Platform-specific attributes
 *
 * Seeds, keys, markers or states need to be stored at a pre-defined memory section
 * which is not initialized during OS startup, that means, being affected from loading
 * .data or .bss section into RAM and thus, being overwritten. Linker attributes for
 * such memory sections may vary between data plaftorms and are defined by
 * @p PUF_SRAM_ATTRIBUTES for each CPU (family) supported. The allocation takes place
 * in the puf_sram implementation.
 *
 * The memory start address for considered SRAM is implementation-specific for the
 * respective CPU architecture. It is hard-coded in the function call of @p puf_sram_init
 * which is usually called inside the platforms reset handler.
 *
 * The PUF feature needs to be enabled via `FEATURES_PROVIDED += puf_sram` in
 * `Makefile.fetures` of the specific board or board family. This should only be done
 * after memory evaluation.
 *
 * ## Soft-reset detection
 *
 * In order to detect a software reboot without preceding power-off phase, a soft-reset
 * detection mechanism writes a marker memory @p PUF_SRAM_MARKER into SRAM. If the marker
 * is still present after a restart, a soft-reset is expected and the PUF procedure
 * is skipped. Otherwise the memory should be uninitializd which indicates "fresh" state.
 *
 *
 * # Random Seed Generation
 *
 * Use of this feature can be enabled via `USEMODULE += puf_sram`.
 *
 * Uninitialized memory pattern are compressed by the lightweight DEK hash function
 * to generate a high entropy 32-bit integer. The default length @p PUF_SRAM_SEED_RAM_LEN
 * is 1KiB and lead to reasonable results for different platforms and manufacturers.
 * However, it can be overwritten. The random number is stored in a pre-allocated section
 * @p puf_sram_seed. Later when the PRNG is seeded, it utilized this value.
 *
 *
 * # Secret ID Generation
 *
 * Use of this feature can be enabled via `USEMODULE += puf_sram_secret`.
 *
 * A PUF response identifies a device like a human fingerprint. Likewise, additional
 * noise is present which needs to be removed for a reliable construction of the same
 * secret after each restart. This is done by means of error correction schemes.
 *
 * Typical setups require two phases: **Enrollment** (helper data generation) which
 * needs to be executed once for the device lifetime in a trusted environment and
 * **Reconstruction** which is done after each restart of the device in deployment.
 *
 * ## Helper Data Generation
 *
 * Helper data is a device-specific and public piece of data required for later noise
 * removal. Its generation incorporates a reference PUF measurement and thus, it requires
 * a measurement firmware on the specific device.
 *
 * As we can't use printf() before UART is initialized, the measurement firmware copies
 * the uninitialized memory pattern to the end of the RAM. Just when main() starts, the
 * copied pattern is printed.
 *
 * The make target `make flash-puf-helpergen` builds the measurement application and
 * flashes the measurement firmware to your `BOARD`.
 *
 * The make target `make puf-helpergen` opens a serial connection to the board under
 * test. It reads the SRAM reference measurement, calculates the helper data and writes
 * it back to the devices non-volatile memory.
 *
 * ## Secret ID Reconstruction
 *
 * This step takes place during deployment. The secret ID is constructed from a noisy
 * PUF measurement and encorporates the helper data during startup. The reconstructed
 * value is stored at a pre-defined memory section @p puf_sram_id and should be deleted
 * as soon as it has been used for its purpose. There is a convenience function for
 * that @p puf_sram_delete_secret.
 *
 *
 * # Status / Notes / Limitations / Future Work
 *
 * ## SRAM Entropy
 *
 * SRAM PUFs are not only prone for attacks when physical device access is given, they
 * may also be affected by aging processes of memory cells. Furthermore, this technique
 * requires a power-down of the memory at the order of seconds. Thus, generating a new
 * seed is impossible on the fly.
 *
 * In future one might think of (i) a concatenation of different entropy mechanisms to
 * increase reliability and (ii) a PUF based on other hardware variences such as timer jitters.
 *
 * ## Memory Start Addresse
 *
 * It is absolutely mandatory that the SRAM start address between PUF measurement for helper
 * generation during enrollment and reconstruction are the same. This should be given by the
 * default configuration.
 *
 * ## Non-volatile memory
 *
 * As a consequence of the PUF code being executed before actual RIOT initialization,
 * we restricted the non-volatile memory usage to internal EEPROM to refrain from eventually
 * required peripherals to drive SD-cards or similar.
 *
 * The location to write/read the helper data in EEPROM is defined by
 * @p PUF_SRAM_HELPER_EEPROM_START (set to 0 by default) **without** checking for existing
 * data when writing the helper and without verifying this is actually helper data before
 * usage.
 *
 * ## Helper Secrecy
 *
 * The helper data is public and does ideally not reveal information about the PUF itself.
 * However, a proper secrecy analysis has not yet been applied.
 *
 * ## Secret ID Entropy and PUF length
 *
 * One relevant parameter whic determines the entropy of a generated secret is the length
 * of the PUF measurement. The length depends on the code rate of respective error correction
 * schemes as well as the (randomly chosen) code offset. The length of the code offset is
 * defined by @p PUF_SRAM_CODEOFFSET_LEN for the RIOT measurement firmware. With 6 Bytes code
 * offset the resulting PUF length is 132 Bytes. The helper requires the same size
 * on non-volatile memory.
 *
 * The make target `make flash-puf-helpergen` triggers a python script to calculate the helper
 * and is configured to induce 6 Bytes code offset by default. In case @p PUF_SRAM_CODEOFFSET_LEN
 * has been changed in the measurement firmware, the build target needs to be called with a
 * configuration parameter as follows:
 * `make flash-puf-helpergen CODEOFFSET=<number of code offset bytes>`
 *
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
#define PUF_SRAM_SEED_RAM_LEN        (1024)
#endif

/**
 * @brief length of the random code offset for secret generation
          during enrollment. This value determines the helper data
          length and thus, it affects the PUF length. The PUF lenght
          itself affects the secrecy of the generated ID.
 */
#ifndef PUF_SRAM_CODEOFFSET_LEN
#define PUF_SRAM_CODEOFFSET_LEN      (6) // Bytes
#endif

/**
 * @brief length of the encoded secret with golay(24, 12)
 */
#define PUF_SRAM_GOLAY_LEN           (2 * PUF_SRAM_CODEOFFSET_LEN)

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

/* TODO: Remove. Only for debug. */
extern uint8_t codeoffset_debug[6];
extern uint8_t ram_debug[PUF_SRAM_HELPER_LEN];
extern uint8_t helper_debug[PUF_SRAM_HELPER_LEN];



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
