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
#include <stdbool.h>

#include "puf_sram.h"
#include "shell.h"
#include "random.h"
#include "ecc/golay2412.h"
#include "ecc/repetition.h"
#include "periph/eeprom.h"

#define GEN_FLAG_EEPROM_POS     (PUF_SRAM_HELPER_LEN + 1)

static inline void _print_buf(uint8_t *buf, size_t len, char *title)
{
    printf("%s\n", title);
    printf("\n[");
    for (unsigned i = 0; i < len; i++) {
        /*if (i % 10 == 0) {
            printf("\n");
        }*/

        if (i == (len - 1)) {
            printf("0x%02x", buf[i]);
        } else {
            printf("0x%02x, ", buf[i]);
        }
    }
    printf("]\n");
}

static inline void _set_gen_flag(void)
{
    uint8_t data[1] = {1};
    printf("_set_gen_flag()\n");
    eeprom_write(GEN_FLAG_EEPROM_POS, data, 1);
}

static inline void _clr_gen_flag(void)
{
    uint8_t data[1] = {0};
    printf("_clr_gen_flag()\n");
    eeprom_write(GEN_FLAG_EEPROM_POS, data, 1);
}

static inline bool _read_gen_flag(void)
{
    uint8_t data[1] = {0};
    printf("_read_gen_flag()\n");
    eeprom_read(GEN_FLAG_EEPROM_POS, data, 1);
    printf("flag: %d\n", data[0]);
    return (data[0] == 1) ? true : false;
}

static inline void _enrollment(void)
{
    static uint8_t helper[PUF_SRAM_HELPER_LEN] = {0};
    static uint8_t golay[PUF_SRAM_GOLAY_LEN] = {0};
    static uint8_t repetition[PUF_SRAM_HELPER_LEN] = {0};
    /* XXX: Fixed values for debug purposes. Will change later. */
    static uint8_t bytes[PUF_SRAM_CODEOFFSET_LEN] = {1, 1, 1, 1, 1, 1};

    puts("_enrollment() - start");
    printf("puf_sram_seed: [0x%08lX]\n", puf_sram_seed);

    /* TODO: Add support for multiple iterations.
          - Sum up in binary representation (if needed)
          - Binarize maximum to likelihood
          - Build the enr_bytes together again
          NOTE: The very first init of puf_sram_seed is the first iteration.
          Just hold it simple for the moment.
          dist/tools/puf-sram/puf_sram.py:56 */
    //uint32_t mle_response = puf_sram_seed;
    uint8_t *ref_mes = (uint8_t *)&puf_sram_seed;

    /* Write random bytes in range(0, 255) into bytes.
       NOTE: seed generated on sys/random/random.c:49 with
       random_init(puf_sram_seed).
       XXX: Fixed values are used ATM. */
    //random_bytes(bytes, PUF_SRAM_CODEOFFSET_LEN);

    golay2412_encode(PUF_SRAM_CODEOFFSET_LEN, bytes, golay);

    repetition_encode(PUF_SRAM_GOLAY_LEN, golay, repetition);

    for(unsigned i = 0; i < PUF_SRAM_HELPER_LEN; i++) {
        helper[i] = repetition[i] ^ ref_mes[i];
    }

    _print_buf(ref_mes, PUF_SRAM_HELPER_LEN, "ENR - ref_mes:");

    /* Save the generated helper data to non-volatile memory.
       NOTE: For the moment, it is restricted to EEPROM. */
    eeprom_write(PUF_SRAM_HELPER_EEPROM_START, helper, PUF_SRAM_HELPER_LEN);

    _print_buf(helper, PUF_SRAM_HELPER_LEN, "ENR - helper:");

    puts("_enrollment() - done");
}

static inline void _reconstruction(void)
{
    puts("_reconstruction() - start");

    printf("ID [0x");

    for (unsigned i = 0; i < sizeof(puf_sram_id); i++){
        printf("%x", puf_sram_id[i]);
    }

    printf("]\n");

    printf("Codeoffset:\n");

    for (unsigned i = 0; i < 6; i++) {
        printf("[%d] = %d\n", i, codeoffset_debug[i]);
    }

    puts("_reconstruction() - done");
}

static int cmd_gen_set(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    _set_gen_flag();
    return 0;
}

static int cmd_gen_clr(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    _clr_gen_flag();
    return 0;
}

static int cmd_gen_read(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    _read_gen_flag();
    return 0;
}

static const shell_command_t shell_commands[] = {
    { "set", "Generate helper data on next coldboot", cmd_gen_set },
    { "clr", "Clear eeprom flag", cmd_gen_clr },
    { "read", "read eeprom flag", cmd_gen_read },
    { NULL, NULL, NULL }
};

int main(void)
{
    puts("\nRIOT/tests/puf-sram-node/");

#ifdef PUF_SRAM_GEN_HELPER
    printf("PUF_SRAM_GEN_HELPER\n");
    if (_read_gen_flag()) {
        _enrollment();
        _clr_gen_flag();
    }
#else

#ifdef MODULE_PUF_SRAM_SECRET
    printf("MODULE_PUF_SRAM_SECRET\n");
    _reconstruction();

    // TODO: Remove. Only for debug.
    _print_buf(helper_debug, PUF_SRAM_HELPER_LEN, "REC - helper_debug:");
    _print_buf(ram_debug, PUF_SRAM_HELPER_LEN, "REC - ram_debug:");
#endif

#endif

    /* run the shell */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
