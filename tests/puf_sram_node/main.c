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

/* The position in eeprom to write the helper gen flag. */
#define GEN_FLAG_EEPROM_POS     (PUF_SRAM_HELPER_LEN + 1)

static inline void _print_buf(uint8_t *buf, size_t len, char *title)
{
    printf("%s\n", title);
    printf("[");
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
    printf("]\n\n");
}

static inline void _set_helper_gen_flag(void)
{
    static uint8_t eeprom_io[1] = {1};
    eeprom_write(GEN_FLAG_EEPROM_POS, eeprom_io, 1);
    puts("Helper flag set.");
}

static inline void _clr_helper_gen_flag(void)
{
    static uint8_t eeprom_io[1] = {0};
    eeprom_write(GEN_FLAG_EEPROM_POS, eeprom_io, 1);
    puts("Helper flag cleared.");
}

static inline bool _read_helper_gen_flag(void)
{
    static uint8_t eeprom_io[1] = {0};
    eeprom_read(GEN_FLAG_EEPROM_POS, eeprom_io, 1);
    return (eeprom_io[0] == 1) ? true : false;
}

static inline void _reconstruction(void)
{
    // TODO:
    uint8_t *ref_mes = (uint8_t *)&puf_sram_seed;

    puf_sram_generate_secret(ref_mes);

    puts("Secret generated.");

    //_print_buf(helper_debug, PUF_SRAM_HELPER_LEN, "REC - helper_debug(puf_sram.c):");

    _print_buf(puf_sram_id, sizeof(puf_sram_id), "REC - puf_sram_id:");

    _print_buf(codeoffset_debug, 6, "REC - codeoffset_debug:");
}

static inline void _enrollment(void)
{
    static uint8_t helper[PUF_SRAM_HELPER_LEN] = {0};
    static uint8_t golay[PUF_SRAM_GOLAY_LEN] = {0};
    static uint8_t repetition[PUF_SRAM_HELPER_LEN] = {0};
    /* TODO: Fixed values for debug purposes. Will change later. */
    static uint8_t bytes[PUF_SRAM_CODEOFFSET_LEN] = {1, 1, 1, 1, 1, 1};
    //random_bytes(bytes, PUF_SRAM_CODEOFFSET_LEN);

    /* TODO: Add support for multiple iterations.
          - Sum up in binary representation (if needed)
          - Binarize maximum to likelihood
          - Build the enr_bytes together again
          NOTE: The very first init of puf_sram_seed is the first iteration.
          Just hold it simple for the moment.
          dist/tools/puf-sram/puf_sram.py:56 */
    //uint32_t mle_response = puf_sram_seed;

    uint8_t *ref_mes = (uint8_t *)&puf_sram_seed;

    golay2412_encode(PUF_SRAM_CODEOFFSET_LEN, bytes, golay);

    repetition_encode(PUF_SRAM_GOLAY_LEN, golay, repetition);

    for(unsigned i = 0; i < PUF_SRAM_HELPER_LEN; i++) {
        helper[i] = repetition[i] ^ ref_mes[i];
    }

    eeprom_write(PUF_SRAM_HELPER_EEPROM_START, helper, PUF_SRAM_HELPER_LEN);

    //eeprom_read(PUF_SRAM_HELPER_EEPROM_START, helper, PUF_SRAM_HELPER_LEN);

    //_print_buf(helper, PUF_SRAM_HELPER_LEN, "ENR - helper(read):");

    puts("Helper data generated and saved in eeprom.");
}

static int cmd_helper_flag_set(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    _set_helper_gen_flag();
    return 0;
}

static int cmd_helper_flag_clr(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    _clr_helper_gen_flag();
    return 0;
}

static int cmd_helper_flag_read(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    printf("Helper flag set? [%s]\n", ((_read_helper_gen_flag() == true) ? "True" : "False"));
    return 0;
}

static int cmd_gen_key(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    _reconstruction();
    return 0;
}

static const shell_command_t shell_commands[] = {
    { "set", "Set EEPROM helper-gen-flag.", cmd_helper_flag_set },
    { "clr", "Clear EEPROM helper-gen-flag.", cmd_helper_flag_clr },
    { "read", "Read EEPROM helper-gen-flag.", cmd_helper_flag_read },
    { "key", "Generate a secret key.", cmd_gen_key },
    { NULL, NULL, NULL }
};

int main(void)
{
#ifdef PUF_SRAM_GEN_HELPER
    if (_read_helper_gen_flag()) {
        puts("Previous set helper gen flag detected.");
        _enrollment();
        _clr_helper_gen_flag();
    }
#else

#ifdef MODULE_PUF_SRAM_SECRET
    puts("MODULE_PUF_SRAM_SECRET");
    _reconstruction();
    puf_sram_delete_secret();
    puts("Secret deleted.");
#endif

#endif
    /* Run the shell. */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}
