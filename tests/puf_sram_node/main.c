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

#ifdef MODULE_PM_LAYERED
#include "pm_layered.h"
#endif

#define FLAG_SIZE                   (1) // 1 byte size..

#ifdef USE_EEPROM
#include "periph/eeprom.h"
#define EEPROM_FLAG_POS             (PUF_SRAM_HELPER_LEN + FLAG_SIZE)
#endif

#ifdef USE_N25Q128
#include "n25q128.h"
static n25q128_dev_t n25q128;
#define HELPER_N25Q128_ADDR         (0x0) // Sektor 0
#define HELPER_FLAG_N25Q128_ADDR    (0x20000) // Sektor 2
#endif

#define ID_LOOPS_FLAG_ADDR          (0x30000) // Sektor 3
#define ID_LOOPS_COUNTER_ADDR       (0x40000) // Sektor 4
#define ID_LOOPS_N_ADDR             (0x50000) // Sektor 5

#define ENABLE_DEBUG                (0)
#include "debug.h"

#define MAIN_DELAY                  (2) // seconds; xtimer

/** HELPER FUNCTIONS **/

static inline void _print_id(void)
{
    printf("idstart{ ");
    for (unsigned i = 0; i < sizeof(puf_sram_id); i++) {
        printf("%d ", puf_sram_id[i]);
    }
    printf("}idend\n");
}

/* @brief   Prints buffer like helper data or ID to the console. */
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

static inline void _set_flag(uint32_t flag_addr)
{
    static uint8_t flag[FLAG_SIZE] = {1};
#ifdef USE_EEPROM
    eeprom_write(EEPROM_FLAG_POS, flag, FLAG_SIZE);
#else
#ifdef USE_N25Q128
    n25q128_page_program(&n25q128, flag_addr, flag, FLAG_SIZE);
    //DEBUG("Flag set\n");
#else
    (void)flag;
#endif /* USE_N25Q128 */
#endif /* USE_EEPROM */
xtimer_usleep(1000U * US_PER_MS);
}

static inline void _clear_flag(uint32_t flag_addr)
{

    static uint8_t flag[FLAG_SIZE] = {0};

#ifdef USE_EEPROM
    eeprom_write(EEPROM_FLAG_POS, flag, FLAG_SIZE);
#else
#ifdef USE_N25Q128
    n25q128_sector_erase(&n25q128, flag_addr);
    xtimer_usleep(1000U * US_PER_MS);
    (void)flag;
#endif /* USE_N25Q128 */
#endif /* USE_EEPROM */

}

static inline int _is_flag_set(uint32_t flag_addr)
{
    static uint8_t flag[FLAG_SIZE] = {0};
#ifdef USE_EEPROM
    eeprom_read(EEPROM_FLAG_POS, flag, FLAG_SIZE);
#else
#ifdef USE_N25Q128
    n25q128_read_data_bytes(&n25q128, flag_addr, flag, FLAG_SIZE);
#else
    (void)flag;
#endif /* USE_N25Q128 */
#endif /* USE_EEPROM */
    return (flag[0] == 1);
}

static inline void _gen_helper_data(void)
{
    static uint8_t helper[PUF_SRAM_HELPER_LEN] = {0};
    static uint8_t golay[PUF_SRAM_GOLAY_LEN] = {0};
    static uint8_t repetition[PUF_SRAM_HELPER_LEN] = {0};
    static uint8_t bytes[PUF_SRAM_CODEOFFSET_LEN] = {0};

    random_bytes(bytes, PUF_SRAM_CODEOFFSET_LEN);
    //_print_buf(bytes, PUF_SRAM_CODEOFFSET_LEN, "\t- Codeoffset(main.c):");

    uint8_t *ref_mes = (uint8_t *)&puf_sram_seed;

    golay2412_encode(PUF_SRAM_CODEOFFSET_LEN, bytes, golay);

    repetition_encode(PUF_SRAM_GOLAY_LEN, golay, repetition);

    for(unsigned i = 0; i < PUF_SRAM_HELPER_LEN; i++) {
        helper[i] = repetition[i] ^ ref_mes[i];
    }

    DEBUG("\(!)Helper data generated and stored(!)\n");

#ifdef USE_EEPROM
    eeprom_write(PUF_SRAM_HELPER_EEPROM_START, helper, PUF_SRAM_HELPER_LEN);
    //DEBUG("\t- Helper data written into EEPROM\n");
    _set_flag();
#else

#ifdef USE_N25Q128
    n25q128_page_program(&n25q128, HELPER_N25Q128_ADDR, helper, PUF_SRAM_HELPER_LEN);
    //DEBUG("\t- Helper data written into N25Q128\n");
    // The n25q128 needs a few moment before writing the next one.
    xtimer_usleep(1000U * US_PER_MS);
    _set_flag(HELPER_FLAG_N25Q128_ADDR);
#endif /* USE_N25Q128 */

#endif /* USE_EEPROM */
}

static inline void _going_into_standby(int seconds)
{
    int duration = seconds;

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
    if (duration > 60) {
        printf("Set duration to 60 seconds\n");
        duration = 60;
    }

    time.tm_sec += duration;

    if (time.tm_sec >= 60) {
        time.tm_sec -= 60;
        time.tm_min += 1;
    }

    DEBUG("Set RTC alarm to fire in ~%d seconds, to wake up from standby.\n", duration);
    rtc_set_alarm(&time, 0, 0);

    DEBUG("Going into standby now.\n");
    pm_set(STM32_PM_STANDBY);
}

static int cmd_helper(int argc, char **argv)
{
    if (argc > 2 || argc < 1) {
        DEBUG("usage: helper [clear]\n");
        return 1;
    }

    if (strcmp(argv[1], "clear") == 0) {
#ifdef USE_EEPROM
        eeprom_clear(PUF_SRAM_HELPER_EEPROM_START, PUF_SRAM_HELPER_LEN);
        DEBUG("EEPROM: %d bytes erased from position %d\n", PUF_SRAM_HELPER_LEN, PUF_SRAM_HELPER_EEPROM_START);
        /* To signalize that the helper data is notid_loops_counter generated. */
        _clear_flag(EEPROM_FLAG_POS);
#else

#ifdef USE_N25Q128
        n25q128_sector_erase(&n25q128, HELPER_N25Q128_ADDR);
        //DEBUG("N25Q128: Sector on address <0x%06x> cleared!\n", HELPER_N25Q128_ADDR);
        /* Sector erase commands need a few moment, before next erase can be processed. */
        xtimer_sleep(2);
        /* To signalize that the helper data is not generated (because it was erased). */
        _clear_flag(HELPER_FLAG_N25Q128_ADDR);
        DEBUG("(!)Helper data erased(!)\n");
#endif /* USE_N25Q128 */

#endif /* USE_EEPROM */
    }

    return 0;
}

static int cmd_standby(int argc, char **argv)
{
    if (argc > 2 || argc < 2) {
        DEBUG("usage: standby <duration_in_seconds>\n");
        return 1;
    }
    _going_into_standby(atoi(argv[1]));
    return 0;
}

static int cmd_id(int argc, char **argv)
{
    if (argc > 2 || argc < 1) {
        DEBUG("usage: id [gen]\n");
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

    n25q128_sector_erase(&n25q128, HELPER_N25Q128_ADDR);
    xtimer_usleep(1000U * US_PER_MS);

    n25q128_sector_erase(&n25q128, ID_LOOPS_FLAG_ADDR);
    xtimer_usleep(1000U * US_PER_MS);

    n25q128_sector_erase(&n25q128, ID_LOOPS_COUNTER_ADDR);
    xtimer_usleep(1000U * US_PER_MS);

    n25q128_sector_erase(&n25q128, ID_LOOPS_N_ADDR);
    xtimer_usleep(1000U * US_PER_MS);

    // Tooks a couple of minutes ~250 seconds...
    //n25q128_bulk_erase(&n25q128);
    printf("done..\n");
    return 0;
}

static int cmd_id_loop(int argc, char **argv)
{
    static uint8_t num_of_buf[1] = {0};
    static uint8_t counter[1] = {1};

    if (argc > 2 || argc < 1) {
        DEBUG("usage: idloop <number of low-power cycles>");
        return 1;
    }

    //printf("\n\n cmd_id_loop: setting all up... wait a few seconds\n");

    num_of_buf[0] = atoi(argv[1]);

    // ERASE all needed sectors, just to be sure.
    n25q128_sector_erase(&n25q128, HELPER_N25Q128_ADDR);
    xtimer_usleep(1000U * US_PER_MS);

    n25q128_sector_erase(&n25q128, ID_LOOPS_FLAG_ADDR);
    xtimer_usleep(1000U * US_PER_MS);

    n25q128_sector_erase(&n25q128, ID_LOOPS_COUNTER_ADDR);
    xtimer_usleep(1000U * US_PER_MS);

    n25q128_sector_erase(&n25q128, ID_LOOPS_N_ADDR);
    xtimer_usleep(1000U * US_PER_MS);

    _clear_flag(HELPER_FLAG_N25Q128_ADDR);

    // So the device know it should loop..
    _set_flag(ID_LOOPS_FLAG_ADDR);

    // Set the counter to 0.
    n25q128_page_program(&n25q128, ID_LOOPS_COUNTER_ADDR, counter, 1);
    xtimer_usleep(1000U * US_PER_MS);

    // program the number of low-power-cycles.
    n25q128_page_program(&n25q128, ID_LOOPS_N_ADDR, num_of_buf, 1);
    xtimer_usleep(1000U * US_PER_MS);

    _going_into_standby(1);

    return 0;
}

static const shell_command_t shell_commands[] = {
    { "standby", "standby <duration_in_seconds>; Triggers enrollment, if helper data is not available", cmd_standby },
    { "helper", "helper [clear]; Shows or clear the helper data", cmd_helper },
    { "id", "id [gen|clear]; Show, generate or delete the ID (need to enroll before!)", cmd_id },
    { "idloop", "idloop <num_of_low-power_clycle> Generates ID's over a specific amount of low-power cycles", cmd_id_loop },
    { "bulkerase", "bla", cmd_bulkerase },
    { NULL, NULL, NULL }
};

int main(void)
{
    /* Sleep a few seconds to wait for settig up the UART. */
    xtimer_sleep(MAIN_DELAY);

#ifdef USE_N25Q128
    /* XXX: Specific configuration for iotlab-m3 nodes. */
    n25q128.conf.bus = EXTFLASH_SPI;
    n25q128.conf.mode = SPI_MODE_0;
    n25q128.conf.clk = SPI_CLK_100KHZ;
    n25q128.conf.cs = EXTFLASH_CS;
    n25q128.conf.write = EXTFLASH_WRITE;
    n25q128.conf.hold = EXTFLASH_HOLD;
    if ((n25q128_init(&n25q128) != 0)) {
        DEBUG("Initialize flash memory driver: N25Q128 [ FAILED! ]\n");
    }
#endif /* USE_N25Q128 */

    /*
    if (power_cycle_detected) {
        DEBUG("Power cycle detected:\n");
        if (!_is_flag_set()) {
            _gen_helper_data();
        }
        puf_sram_generate_secret((uint8_t *)&puf_sram_seed);
        //_print_buf(codeoffset_debug, PUF_SRAM_CODEOFFSET_LEN, "\t- Codeoffset(puf_sram.c):");
        _print_id();
    } else {
        printf("Softreset detected:\n");
        printf("\t-> waiting for next power cycle (replug the power source!)\n");
    }
    */

    if (power_cycle_detected){

        //printf("\n\nLOW-POWER CYCLE:\n");

        if (_is_flag_set(ID_LOOPS_FLAG_ADDR)) {

            //printf("Loop-Flag is set\n");

            static uint8_t n[1] = {0};
            static uint8_t counter[1] = {0};

            n25q128_read_data_bytes(&n25q128, ID_LOOPS_N_ADDR, n, 1);
            xtimer_usleep(1000U * US_PER_MS);
            //printf("num of low-power cycles: %d\n", n[0]);

            n25q128_read_data_bytes(&n25q128, ID_LOOPS_COUNTER_ADDR, counter, 1);
            xtimer_usleep(1000U * US_PER_MS);
            //printf("cycle iteration no.: %d\n", counter[0]);

            // GEN HELPER; ID; PRINT
            if (!_is_flag_set(HELPER_FLAG_N25Q128_ADDR)) {
                _gen_helper_data();
                puf_sram_generate_secret((uint8_t *)&puf_sram_seed);
                _print_id();
            }

            // Erase helper data
            n25q128_sector_erase(&n25q128, HELPER_N25Q128_ADDR);
            xtimer_usleep(1000U * US_PER_MS);
            _clear_flag(HELPER_FLAG_N25Q128_ADDR);
            DEBUG("(!)Helper data erased(!)\n");


            /* Stop, if we reached the amount of low-power-cycles. */
            if (counter[0] >= n[0]) {
                printf("\nDone...\n");
                _clear_flag(ID_LOOPS_FLAG_ADDR);
                return 0;
            }

            uint32_t cnt = counter[0];
            cnt += 1;
            counter[0] = cnt;

            // Needed to increment the counter. First erase, then program...
            n25q128_sector_erase(&n25q128, ID_LOOPS_COUNTER_ADDR);
            xtimer_usleep(1000U * US_PER_MS);

            n25q128_page_program(&n25q128, ID_LOOPS_COUNTER_ADDR, counter, 1);
            xtimer_usleep(1000U * US_PER_MS);

            _going_into_standby(1);
        }
    } else {
        printf("Softreset detected: waiting for next power cycle (replug the power source!\n");
    }

    /* Running the shell. */
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
