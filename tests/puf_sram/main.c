/*
 * Copyright (C) 2018 HAW Hamburg
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
 * @brief       Test application for the random number generator based of SRAM
 *
 * @author      Kevin Weiss <kevin.weiss@haw-hamburg.de>
 *
 * @}
 */
#include <inttypes.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "shell.h"
#include "puf_sram.h"
#include "periph/eeprom.h"

#ifdef PUF_SRAM_GEN_HELPER

#define BUFSIZE (PUF_SRAM_HELPER_LEN)
uint8_t eeprom_io[BUFSIZE];

static int cmd_write(int argc, char **argv)
{
    if (argc < 3) {
        printf("usage: %s <pos> <data>\n", argv[0]);
        return 1;
    }
    uint32_t pos = atoi(argv[1]);
    uint8_t data_len = argc -2;

    for(int i=0; i<data_len; i++) {
        eeprom_io[i]=atoi(argv[i+2]);
    }
    eeprom_write(pos, eeprom_io, data_len);

    return 0;
}
static const shell_command_t shell_commands[] = {
    { "write", "Write bytes to eeprom", cmd_write },
    { NULL, NULL, NULL }
};
#endif

int main(void)
{
    puts("Start: PUF features");
#ifdef PUF_SRAM_GEN_HELPER
    printf("Success: Data for reference PUF: [");
    uint8_t *addr = (uint8_t *)puf_sram_seed;
    for (unsigned i=0;i<PUF_SRAM_HELPER_LEN;i++){
        printf("0x%02x ", addr[i]);
    }
    puts("]End: Test finished");
    /* run the shell */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
#else
    printf("Success: Data for puf_sram_seed: [0x%08lX", puf_sram_seed);
#ifdef MODULE_PUF_SRAM_SECRET
    printf(" 0x");
    for (unsigned i=0;i<sizeof(puf_sram_id);i++){
        printf("%x", puf_sram_id[i]);
    }
    puf_sram_delete_secret();
#endif
    puts("]End: Test finished");
    return 0;
#endif

    return 0;
}
