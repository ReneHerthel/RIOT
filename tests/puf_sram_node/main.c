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

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h> // atoi
//#include <time.h>

#include "periph_cpu_common.h"
#include "periph_conf.h"
#include "board.h"
#include "ecc/repetition.h"
#include "ecc/golay2412.h"
#include "random.h"
#include "xtimer.h"
#include "timex.h"
#include "periph/rtc.h"
#include "periph/pm.h"
#include "puf_sram.h"
#include "shell.h"

#ifdef MODULE_PM_LAYERED
#include "pm_layered.h"
#endif

#include "n25q128.h"
static n25q128_dev_t n25q128;
#define HELPER_N25Q128_ADDR         (0x0)     // Sektor 0
#define HELPER_FLAG_N25Q128_ADDR    (0x20000) // Sektor 2

#define ID_LOOPS_FLAG_ADDR          (0x30000) // Sektor 3
#define ID_LOOPS_ITER_ADDR          (0x40000) // Sektor 4
#define ID_LOOPS_N_ADDR             (0x50000) // Sektor 5
#define ID_LOOPS_SIZE               (4)       // in bytes

#define FLAG_SIZE                   (1)       // in byte

#define STANDBY_TIME                (1)       // in seconds

#define ENABLE_DEBUG                (0)
#include "debug.h"

/* clamps a specifc value between minimum and maximum */
#define CLAMP(x, min, max)    ((x > max) ? max : ((x < min) ? min : x))

static inline void _print_buf(uint8_t *buf, size_t len, char *title)
{
    DEBUG("\n%s\n", title);
    DEBUG("\t[");
    for (unsigned i = 0; i < len; i++) {
        if (i == (len - 1)) {
            DEBUG("%d", buf[i]); //DEBUG("0x%02x", buf[i]);
        } else {
            DEBUG("%d, ", buf[i]); //DEBUG("0x%02x, ", buf[i]);
        }
    }
    DEBUG("]\n\n");
}

static inline void _wait_for_wip(void)
{
    while(n25q128_write_in_progress(&n25q128));
}

static inline void _wait_for_controller(void)
{
    while(n25q128_program_erase_status(&n25q128) == 0);
}

static inline void _print_id(void)
{
    uint8_t lp_iter[ID_LOOPS_SIZE] = {0};

    _wait_for_controller();
    n25q128_read_data_bytes(&n25q128, ID_LOOPS_ITER_ADDR, lp_iter, ID_LOOPS_SIZE);

    uint32_t i = 0;
    i = lp_iter[0] | (lp_iter[1] << 8) | (lp_iter[2] << 16) | (lp_iter[3] << 24);

    printf("No.: %ld \tidstart{ ", i);
    for (unsigned i = 0; i < sizeof(puf_sram_id); i++) {
        printf("%d ", puf_sram_id[i]);
    }
    printf("}idend\n");
}

static inline void _sector_erase(uint32_t addr)
{
    _wait_for_controller();
    n25q128_sector_erase(&n25q128, addr);
    _wait_for_wip();
}

static inline void _page_program(uint32_t addr, uint8_t * out, size_t len)
{
    _wait_for_controller();
    n25q128_page_program(&n25q128, addr, out, len);
    _wait_for_wip();
}

static inline void _read_bytes(uint32_t addr, uint8_t * in, size_t len)
{
    _wait_for_controller();
    n25q128_read_data_bytes(&n25q128, addr, in, len);
}

static inline void _set_flag(uint32_t flag_addr)
{
    static uint8_t flag[FLAG_SIZE] = {1};
    _page_program(flag_addr, flag, FLAG_SIZE);
}

static inline void _clear_flag(uint32_t flag_addr)
{
    _sector_erase(flag_addr);
}

static inline int _is_flag_set(uint32_t flag_addr)
{
    static uint8_t flag[FLAG_SIZE] = {0};
    _wait_for_controller();
    n25q128_read_data_bytes(&n25q128, flag_addr, flag, FLAG_SIZE);
    return (flag[0] == 1);
}

static inline void _gen_helper_data(void)
{
    static uint8_t helper[PUF_SRAM_HELPER_LEN] = {0};
    static uint8_t golay[PUF_SRAM_GOLAY_LEN] = {0};
    static uint8_t repetition[PUF_SRAM_HELPER_LEN] = {0};
    static uint8_t bytes[PUF_SRAM_CODEOFFSET_LEN] = {0};
    uint8_t *ref_mes = (uint8_t *)&puf_sram_seed;

    random_bytes(bytes, PUF_SRAM_CODEOFFSET_LEN);
    golay2412_encode(PUF_SRAM_CODEOFFSET_LEN, bytes, golay);
    repetition_encode(PUF_SRAM_GOLAY_LEN, golay, repetition);

    for(unsigned i = 0; i < PUF_SRAM_HELPER_LEN; i++) {
        helper[i] = repetition[i] ^ ref_mes[i];
    }

    _page_program(HELPER_N25Q128_ADDR, helper, PUF_SRAM_HELPER_LEN);
    _set_flag(HELPER_FLAG_N25Q128_ADDR);
    DEBUG("(!)Helper data generated and stored(!)\n");
    //_print_buf(bytes, PUF_SRAM_CODEOFFSET_LEN, "\t- Codeoffset(main.c):");
}

static inline void _going_into_standby(int seconds)
{
    struct tm time = {
        .tm_year = 2020 - 1900,   /* years are counted from 1900 */
        .tm_mon  = 1,             /* 0 = January, 11 = December */
        .tm_mday = 1,
        .tm_hour = 1,
        .tm_min  = 1,
        .tm_sec  = 1
    };

    rtc_set_time(&time);

    /* No need to sleep longer than 60 seconds in this test. */
    time.tm_sec += CLAMP(seconds, 0, 60);;

    if (time.tm_sec >= 60) {
        time.tm_sec -= 60;
        time.tm_min += 1;
    }

    rtc_set_alarm(&time, 0, 0);

    pm_set(STM32_PM_STANDBY);
}

static int cmd_helper(int argc, char **argv)
{
    if (argc > 2 || argc < 1) {
        DEBUG("usage: helper <clear>\n");
        return 1;
    }

    if (strcmp(argv[1], "clear") == 0) {
        _sector_erase(HELPER_N25Q128_ADDR);
        _clear_flag(HELPER_FLAG_N25Q128_ADDR);
        DEBUG("\t>> Helper data cleared!\n");
    }

    return 0;
}

static int cmd_standby(int argc, char **argv)
{
    int duration = 0;

    if (argc > 2 || argc < 2) {
        DEBUG("usage: standby <duration_in_seconds>\n");
        return 1;
    }

    duration = atoi(argv[1]);

    _going_into_standby(duration);

    return 0;
}

static int cmd_id(int argc, char **argv)
{
    if (argc > 2 || argc < 1) {
        DEBUG("usage: id <gen|clear>\n");
        return 1;
    }

    if (strcmp(argv[1], "gen") == 0) {
        puf_sram_generate_secret((uint8_t *)&puf_sram_seed);
        DEBUG("Secret generated\n");
    } else if (strcmp(argv[1], "clear") == 0) {
        puf_sram_delete_secret();
        DEBUG("Secret cleared\n");
    }
    //_print_buf(puf_sram_id, sizeof(puf_sram_id), "cmd_id - ID:");
    //_print_buf(codeoffset_debug, PUF_SRAM_CODEOFFSET_LEN, "cmd_id - Codeoffset(puf_sram.c):");
    return 0;
}

static int cmd_bulkerase(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    _wait_for_controller();
    n25q128_bulk_erase(&n25q128); // Very slow. Takes few minutes.. (~250sec)
    _wait_for_wip();

    return 0;
}

static int cmd_id_loop(int argc, char **argv)
{
    static uint8_t lp_n[ID_LOOPS_SIZE] = {0};    // The amount of low power cycles
    static uint8_t lp_iter[ID_LOOPS_SIZE] = {0}; // The current iteration

    if (argc > 3 || argc < 1) {
        puts("usage: idloop <num_of_iterations> <y|n>");
        return 1;
    }

    if (strcmp(argv[2], "y") == 0) {
        _sector_erase(HELPER_N25Q128_ADDR);
        _clear_flag(HELPER_FLAG_N25Q128_ADDR);
    } else if (strcmp(argv[2], "n") == 0) {
        // nothing.
    } else {
        puts("usage: idloop <N> <y|n>");
        return 1;
    }

    uint32_t n = (uint32_t)atoi(argv[1]);

    lp_n[0] = (n & 0xFF000000) >> 24;
    lp_n[1] = (n & 0x00FF0000) >> 16;
    lp_n[2] = (n & 0x0000FF00) >> 8;
    lp_n[3] = (n & 0x000000FF);

    // setup flash memory for low power cycle loops
    _sector_erase(ID_LOOPS_FLAG_ADDR);
    _sector_erase(ID_LOOPS_ITER_ADDR);
    _sector_erase(ID_LOOPS_N_ADDR);
    _set_flag(ID_LOOPS_FLAG_ADDR);

    // program the start values for power cycles into the flash mem
    _page_program(ID_LOOPS_ITER_ADDR, lp_iter, ID_LOOPS_SIZE);
    _page_program(ID_LOOPS_N_ADDR, lp_n, ID_LOOPS_SIZE);

    _wait_for_controller();
    _going_into_standby(STANDBY_TIME);

    return 0;
}

static const shell_command_t shell_commands[] = {
    { "standby", "standby <seconds>; Triggers enrollment, if helper data is not available", cmd_standby },
    { "helper", "helper <clear>; Shows or clear the helper data", cmd_helper },
    { "id", "id <gen|clear>; Show, generate or delete the ID (need to enroll before!)", cmd_id },
    { "idloop", "idloop <number of Low-Power cycles> <y|n> Generates ID's over a specific amount of low-power cycles", cmd_id_loop },
    { "bulkerase", "Erase the whole memory. Takes few minutes..", cmd_bulkerase },
    { NULL, NULL, NULL }
};

int main(void)
{
    static uint8_t lp_n[ID_LOOPS_SIZE] = {0};
    static uint8_t lp_iter[ID_LOOPS_SIZE] = {0};
    uint32_t n = 0;
    uint32_t i = 0;

    /* XXX: Specific configuration for iotlab-m3 nodes. */
    n25q128.conf.bus = EXTFLASH_SPI;
    n25q128.conf.mode = SPI_MODE_0;
    n25q128.conf.clk = SPI_CLK_100KHZ;
    n25q128.conf.cs = EXTFLASH_CS;
    n25q128.conf.write = EXTFLASH_WRITE;
    n25q128.conf.hold = EXTFLASH_HOLD;

    if ((n25q128_init(&n25q128) != 0)) {
        puts("\n\n\t FAILED to initialize n25q128 flash memory.");
        return 0;
    }

    // REMEMBER: /core/init.c -> LOG-INFO wieder einfügen..
    if (power_cycle_detected){
        if (_is_flag_set(ID_LOOPS_FLAG_ADDR)) {

            _read_bytes(ID_LOOPS_N_ADDR, lp_n, ID_LOOPS_SIZE);

            // 4 bytes of uint8_t to one single uint32_t value..
            n = lp_n[3] | (lp_n[2] << 8) | (lp_n[1] << 16) | (lp_n[0] << 24);

            _read_bytes(ID_LOOPS_ITER_ADDR, lp_iter, ID_LOOPS_SIZE);
            i = lp_iter[0] | (lp_iter[1] << 8) | (lp_iter[2] << 16) | (lp_iter[3] << 24);
            i += 1;

            lp_iter[3] = (i & 0xFF000000) >> 24;
            lp_iter[2] = (i & 0x00FF0000) >> 16;
            lp_iter[1] = (i & 0x0000FF00) >> 8;
            lp_iter[0] = (i & 0x000000FF);

            if (!_is_flag_set(HELPER_FLAG_N25Q128_ADDR)) {
                _gen_helper_data();
            }

            // setup the information needed between low power cycles
            _sector_erase(ID_LOOPS_ITER_ADDR);
            _page_program(ID_LOOPS_ITER_ADDR, lp_iter, ID_LOOPS_SIZE);

            puf_sram_generate_secret((uint8_t *)&puf_sram_seed);
            _print_id();

            // Check if there is a next low-power cycle
            if (i < n) {
                puf_sram_delete_secret();
                _wait_for_controller();
                _going_into_standby(STANDBY_TIME);
            } else {
                _clear_flag(ID_LOOPS_FLAG_ADDR);
                _sector_erase(ID_LOOPS_ITER_ADDR);
                _sector_erase(ID_LOOPS_N_ADDR);
            }
        }
    } else {
        printf("\n\n\t>>Softreset detected: waiting for next power cycle (replug the power source!\n\n");
    }

    /* Running the shell. */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}