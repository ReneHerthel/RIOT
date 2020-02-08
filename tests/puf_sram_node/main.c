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
#include <time.h>

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

#define FLAG_SIZE                   (1) // byte

#ifdef MODULE_PM_LAYERED
#include "pm_layered.h"
#endif

#ifdef USE_EEPROM
#include "periph/eeprom.h"
#define FLAG_EEPROM_POS             (PUF_SRAM_HELPER_LEN + FLAG_SIZE)
#endif

#ifdef USE_N25Q128
#include "n25q128.h"
static n25q128_dev_t n25q128;
#define HELPER_N25Q128_ADDR         (0x0)
#define FLAG_N25Q128_ADDR           (0x20000)
#endif

/* @brief   Prints buffer like helper data or ID to the console. */
static inline void _print_buf(uint8_t *buf, size_t len, char *title)
{
    printf("\n%s\n", title);

    /* Formated for working with python scripts. */
    printf("[");

    for (unsigned i = 0; i < len; i++) {
        if (i == (len - 1)) {
            printf("0x%02x", buf[i]);
        } else {
            printf("0x%02x, ", buf[i]);
        }
    }

    printf("]\n\n");
}

/* @brief   Sets the helper-gen flag. */
static inline void _set_flag(void)
{
    static uint8_t flag[FLAG_SIZE] = {1};

#ifdef USE_EEPROM
    eeprom_write(FLAG_EEPROM_POS, flag, FLAG_SIZE);
#else
    (void)flag;

#ifdef USE_N25Q128
    n25q128_page_program(&n25q128, FLAG_N25Q128_ADDR, flag, FLAG_SIZE);
#endif /* USE_N25Q128 */

#endif /* USE_EEPROM */

    puts("Flag set");
}

/* @brief   Clears the helper-gen flag. */
static inline void _clear_flag(void)
{
    static uint8_t flag[FLAG_SIZE] = {0};

#ifdef USE_EEPROM
    eeprom_write(FLAG_EEPROM_POS, flag, FLAG_SIZE);
#else
    (void)flag;

#ifdef USE_N25Q128
    n25q128_sector_erase(&n25q128, FLAG_N25Q128_ADDR);
#endif

#endif

    puts("Flag cleared");
}

static inline int _is_flag_set(void)
{
    static uint8_t flag[FLAG_SIZE] = {0};

#ifdef USE_EEPROM
    eeprom_read(FLAG_EEPROM_POS, flag, FLAG_SIZE);
#else
    (void)flag;

#ifdef USE_N25Q128
    n25q128_read_data_bytes(&n25q128, FLAG_N25Q128_ADDR, flag, FLAG_SIZE);
#endif /* USE_N25Q128 */

#endif /* USE_EEPROM */

    return (flag[0] == 1);
}

static inline void _gen_helper_data(void)
{
    static uint8_t helper[PUF_SRAM_HELPER_LEN] = {0};
    static uint8_t golay[PUF_SRAM_GOLAY_LEN] = {0};
    static uint8_t repetition[PUF_SRAM_HELPER_LEN] = {0};
    /* TODO: Fixed values for debug purposes. Will change later. */
    static uint8_t bytes[PUF_SRAM_CODEOFFSET_LEN] = {1, 1, 1, 1, 1, 1};
    //random_bytes(bytes, PUF_SRAM_CODEOFFSET_LEN);
    _print_buf(bytes, PUF_SRAM_CODEOFFSET_LEN, "Enrollment - Codeoffset:");

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

    puts("Helper data generated");

#ifdef USE_EEPROM
    eeprom_write(PUF_SRAM_HELPER_EEPROM_START, helper, PUF_SRAM_HELPER_LEN);
    puts("Helper data written into EEPROM");
#else

#ifdef USE_N25Q128
    n25q128_page_program(&n25q128, HELPER_N25Q128_ADDR, helper, PUF_SRAM_HELPER_LEN);
    puts("Helper data written into N25Q128");
#endif /* USE_N25Q128 */

#endif /* USE_EEPROM */
}

static int cmd_helper(int argc, char **argv)
{
    if (argc > 2 || argc < 1) {
        puts("usage: helper [erase]");
        return 1;
    }

    if (strcmp(argv[1], "clear") == 0) {
#ifdef USE_EEPROM
        eeprom_clear(PUF_SRAM_HELPER_EEPROM_START, PUF_SRAM_HELPER_LEN);
        printf("EEPROM: %d bytes erased from position %d\n", PUF_SRAM_HELPER_LEN, PUF_SRAM_HELPER_EEPROM_START);
#else

#ifdef USE_N25Q128
        /* Erase the *whole* sector of the given address. */
        n25q128_sector_erase(&n25q128, HELPER_N25Q128_ADDR);
        /* addr: 0x0 - 0xFFFF is the first sector (0)*/
        printf("N25Q128: Sector on address <0x%06x> cleared!\n", HELPER_N25Q128_ADDR);
        // The N25Q128 needs a few moment to erase a single sector.
        xtimer_sleep(1);
#endif /* USE_N25Q128 */

#endif /* USE_EEPROM */
    }

#ifdef USE_EEPROM
        static uint8_t mem_io[PUF_SRAM_HELPER_LEN] = {0};
        eeprom_read(PUF_SRAM_HELPER_EEPROM_START, mem_io, PUF_SRAM_HELPER_LEN);
        _print_buf(mem_io, PUF_SRAM_HELPER_LEN, "Helper data:");
#else

#ifdef USE_N25Q128
        static uint8_t mem_io[PUF_SRAM_HELPER_LEN] = {0};
        n25q128_read_data_bytes(&n25q128, HELPER_N25Q128_ADDR, mem_io, PUF_SRAM_HELPER_LEN);
        _print_buf(mem_io, PUF_SRAM_HELPER_LEN, "Helper data:");
#endif /* USE_N25Q128 */

#endif /* USE_EEPROM */

    return 0;
}

static int cmd_enroll(int argc, char **argv)
{
    if (argc > 2 || argc < 2) {
        puts("usage: enroll <standby-duration-seconds>");
        return 1;
    }

    struct tm time = {
        .tm_year = 2020 - 1900,   /* years are counted from 1900 */
        .tm_mon  = 1,             /* 0 = January, 11 = December */
        .tm_mday = 1,
        .tm_hour = 1,
        .tm_min  = 1,
        .tm_sec  = 1
    };

    rtc_set_time(&time);

    int duration = atoi(argv[1]);

    /* No need to sleep longer than 60 seconds in this test. */
    if (duration > 60) {
        duration = 60;
    }

    time.tm_sec += duration;

    if (time.tm_sec >= 60) {
        time.tm_sec -= 60;
        time.tm_min += 1;
    }

    printf("Set RTC alarm (~%d seconds) to wake up from standby\n", duration);
    rtc_set_alarm(&time, 0, 0);

    puts("Set a flag to trigger helper data generation next power cycle");
    _set_flag();

    puts("Going into standby now\n.\n.\n.");
    pm_set(STM32_PM_STANDBY);

    return 0;
}

static int cmd_id(int argc, char **argv)
{
    if (argc > 2 || argc < 1) {
        puts("usage: id [gen]");
        return 1;
    }

    if (strcmp(argv[1], "gen") == 0) {
        // TODO: (uint8_t *)&puf_sram_seed needs to be optimized.
        puf_sram_generate_secret((uint8_t *)&puf_sram_seed);
        puts("Secret generated.\n");
    } else if (strcmp(argv[1], "clear") == 0) {
        puf_sram_delete_secret();
        puts("Secret cleared.\n");
    }

    _print_buf(puf_sram_id, sizeof(puf_sram_id), "cmd_id - ID:");
    _print_buf(codeoffset_debug, PUF_SRAM_CODEOFFSET_LEN, "cmd_id - Codeoffset:");

    return 0;
}

static const shell_command_t shell_commands[] = {
    { "enroll", "enroll <standby-duration-seconds>; Triggers helper generation (need power cycle before!)", cmd_enroll },
    { "helper", "helper [clear]; Shows or clear the helper data", cmd_helper },
    { "id", "id [gen|clear]; Show, generate or delete the ID (need to enroll before!)", cmd_id },
    { NULL, NULL, NULL }
};

int main(void)
{
    /* Sleep a few seconds to wait for settig up the UART. */
    xtimer_sleep(1);

    puts("\n\nApplication: puf sram simplification");

#ifdef USE_EEPROM
    puts("Use EEPROM to write helper data into");
#else

#ifdef USE_N25Q128
    puts("Use N25Q128 to write helper data into");
    /* XXX: Specific configuration for iotlab-m3 nodes. */
    n25q128.conf.bus = EXTFLASH_SPI;
    n25q128.conf.mode = SPI_MODE_0;
    n25q128.conf.clk = SPI_CLK_100KHZ;
    n25q128.conf.cs = EXTFLASH_CS;
    n25q128.conf.write = EXTFLASH_WRITE;
    n25q128.conf.hold = EXTFLASH_HOLD;
    printf("Initialize N25Q128 driver: [ %s ]\n", ((n25q128_init(&n25q128) == 0) ? "OK" : "FAIL"));
#endif /* USE_N25Q128 */

#endif /* USE_EEPROM */

    /* 'power_cycle_detected' declared in puf_sram.h */
    if (power_cycle_detected) {
        puts("Power cycle detected");
        /* Check, if the gen helper flag was set in previously power cycle. */
        if (_is_flag_set()) {
            puts("Flag detected");
            _gen_helper_data();
            _clear_flag();
        }
        xtimer_sleep(1);
        puf_sram_generate_secret((uint8_t *)&puf_sram_seed);
        puts("Secret generated\n");
        _print_buf(puf_sram_id, sizeof(puf_sram_id), "Reconstruction - ID:");
        _print_buf(codeoffset_debug, PUF_SRAM_CODEOFFSET_LEN, "Reconstruction - Codeoffset:");
    } else {
        /* During softreset. Instead of using shell, you can just re-plug the power source. */
        puts("Softreset detected");
        puts("\t-> waiting for next power cycle (replug power source)");
        _set_flag();
    }

    /* Running the shell. */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
