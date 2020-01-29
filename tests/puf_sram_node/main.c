/*
 * Copyright (C) 2020 HAW Hamburg
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

#include "board.h"
#include "puf_sram.h"
#include "shell.h"
#include "random.h"
#include "ecc/golay2412.h"
#include "ecc/repetition.h"

/* TODO: The iotlab-m3 takes a while for prints, so we delay the 'start' a bit. */
#include "xtimer.h"
#include "timex.h"

#ifdef USE_EEPROM
#include "periph/eeprom.h"
#define GEN_FLAG_EEPROM_POS         (PUF_SRAM_HELPER_LEN + 1)
#endif

#ifdef USE_N25Q128
#include "n25q128.h"
static n25q128_dev_t n25q128;
#define HELPER_N25Q128_START        (0)
#define HELPER_FLAG_N25Q128_POS     (500)
#endif

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
    static uint8_t flag[1] = {1};

#ifdef USE_EEPROM
    puts("eeprom_write");
    eeprom_write(GEN_FLAG_EEPROM_POS, flag, 1);
#else

  #ifdef USE_N25Q128
    puts("n25q_128_page_program");
    n25q128_page_program(&n25q128, HELPER_FLAG_N25Q128_POS, flag, 1);
  #else
    puts("No non-volatile-memory defined");
    (void)flag;
  #endif

#endif

    puts("Helper flag set.");
}

static inline void _clr_helper_gen_flag(void)
{
    static uint8_t flag[1] = {0};

#ifdef USE_EEPROM
    eeprom_write(GEN_FLAG_EEPROM_POS, flag, 1);
#else

  #ifdef USE_N25Q128
    (void)flag;
    //n25q128_page_program(&n25q128, HELPER_FLAG_N25Q128_POS, flag, 1);
    n25q128_sector_erase(&n25q128, HELPER_FLAG_N25Q128_POS);
  #endif

#endif

    puts("Helper flag cleared.");
}

static inline bool _read_helper_gen_flag(void)
{
    static uint8_t flag[1] = {0};

#ifdef USE_EEPROM
    eeprom_read(GEN_FLAG_EEPROM_POS, flag, 1);
#else

  #ifdef USE_N25Q128
    n25q128_read_data_bytes(&n25q128, HELPER_FLAG_N25Q128_POS, flag, 1);
  #else
    (void)flag;
  #endif

#endif

    return (flag[0] == 1) ? true : false;
}

static inline void _reconstruction(void)
{
    // TODO:
    uint8_t *ref_mes = (uint8_t *)&puf_sram_seed;

    // TODO: This function uses EEPROM. Change it.
    puf_sram_generate_secret(ref_mes);

    puts("Secret generated.");

    _print_buf(helper_debug, PUF_SRAM_HELPER_LEN, "REC - helper_debug(puf_sram.c):");

    _print_buf(puf_sram_id, sizeof(puf_sram_id), "REC - puf_sram_id:");

    _print_buf(codeoffset_debug, 6, "REC - codeoffset_debug:");
}

static inline void _enrollment(void)
{
    static uint8_t helper[PUF_SRAM_HELPER_LEN] = {0};
    static uint8_t helper_read[PUF_SRAM_HELPER_LEN] = {0};
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

#ifdef USE_EEPROM
    eeprom_write(PUF_SRAM_HELPER_EEPROM_START, helper, PUF_SRAM_HELPER_LEN);
    puts("Helper data generated and saved in eeprom.");
#else

  #ifdef USE_N25Q128
    n25q128_page_program(&n25q128, HELPER_N25Q128_START, helper, PUF_SRAM_HELPER_LEN);
    puts("Helper data generated and saved in n25q128 flash memory.");
  #endif

#endif
    _print_buf(helper, PUF_SRAM_HELPER_LEN, "ENR - helper(write):");

#ifdef USE_N25Q128
    n25q128_read_data_bytes(&n25q128, HELPER_N25Q128_START, helper_read, PUF_SRAM_HELPER_LEN);
    _print_buf(helper_read, PUF_SRAM_HELPER_LEN, "ENR - helper(read):");
#endif
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

static int cmd_bulk_erase(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    puts("Bulk erase the n25q128 flash memory. This takes about ~250 seconds!");
    n25q128_bulk_erase(&n25q128);
    return 0;
}

static int cmd_sector_erase(int argc, char **argv)
{
    int addr = 0;
    if (argc < 1) {
        printf("usage: %s <address of the sector>\n", argv[0]);
        return 1;
    }
    addr = atoi(argv[1]);
    n25q128_sector_erase(&n25q128, addr);
    puts("Erased the sector of the given address.");
    return 0;
}

static const shell_command_t shell_commands[] = {
    { "set", "Set EEPROM helper-gen-flag.", cmd_helper_flag_set },
    { "clr", "Clear EEPROM helper-gen-flag.", cmd_helper_flag_clr },
    { "read", "Read EEPROM helper-gen-flag.", cmd_helper_flag_read },
    { "key", "Generate a secret key.", cmd_gen_key },
    { "be", "Erase the whole memory (n25q128). Takes ~250 seconds!", cmd_bulk_erase },
    { "se", "Erase the given sector (n25q128)", cmd_sector_erase },
    { NULL, NULL, NULL }
};

int main(void)
{
    /* Just sleep a second, because the UART is a bit to slow (for prints). */
    xtimer_sleep(3);

    puts("Application: puf_sram_node");

#ifdef USE_N25Q128
    printf("Configure and initialize the n25q128 flash memory.. ");

    n25q128.conf.bus = EXTFLASH_SPI;
    n25q128.conf.mode = SPI_MODE_0;
    n25q128.conf.clk = SPI_CLK_100KHZ;
    n25q128.conf.cs = EXTFLASH_CS;
    n25q128.conf.write = EXTFLASH_WRITE;
    n25q128.conf.hold = EXTFLASH_HOLD;

    if (n25q128_init(&n25q128) == 0) {
        printf("OK\n");
    } else {
        printf("FAIL\n");
    }
#endif

#ifdef PUF_SRAM_GEN_HELPER
    puts("PUF_SRAM_GEN_HELPER");
    if (_read_helper_gen_flag() == true) {
        puts("Previous set helper gen flag detected.");
        _enrollment();
        _clr_helper_gen_flag();
    } else {
        puts("No helper-gen-flag detected.");
    }
#else

#ifdef MODULE_PUF_SRAM_SECRET
    puts("MODULE_PUF_SRAM_SECRET");
    _reconstruction();
    puf_sram_delete_secret();
    puts("Secret deleted.");
#endif

#endif

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
