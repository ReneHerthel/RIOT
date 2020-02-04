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

#ifdef USE_EEPROM
#include "periph/eeprom.h"
#define GEN_FLAG_EEPROM_POS         (PUF_SRAM_HELPER_LEN + 1)
#endif

#ifdef USE_N25Q128
#include "n25q128.h"
static n25q128_dev_t n25q128;
#define HELPER_N25Q128_START        (0x0)
#define HELPER_FLAG_N25Q128_POS     (0x20000)
#endif

/* Just to increment the seconds of the RTC. */
static inline void inc_secs(struct tm *time, unsigned val)
{
    time->tm_sec += val;
    if (time->tm_sec >= 60) {
        time->tm_sec -= 60;
    }
}

static inline void _print_buf(uint8_t *buf, size_t len, char *title)
{
    printf("%s\n", title);
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

static inline void _set_flag(void)
{
    static uint8_t flag[1] = {1};

#ifdef USE_EEPROM
    eeprom_write(GEN_FLAG_EEPROM_POS, flag, 1);
#else
    (void)flag;

#ifdef USE_N25Q128
    n25q128_page_program(&n25q128, HELPER_FLAG_N25Q128_POS, flag, 1);
#endif /* USE_N25Q128 */

#endif /* USE_EEPROM */

    puts("Flag set");
}

static inline void _clear_flag(void)
{
    static uint8_t flag[1] = {0};
#ifdef USE_EEPROM
    eeprom_write(GEN_FLAG_EEPROM_POS, flag, 1);
#else
    (void)flag;
#ifdef USE_N25Q128
    n25q128_sector_erase(&n25q128, HELPER_FLAG_N25Q128_POS);
#endif
#endif

    puts("Flag cleared");
}

static inline int _is_flag_set(void)
{
    static uint8_t flag[1] = {0};

#ifdef USE_EEPROM
    eeprom_read(GEN_FLAG_EEPROM_POS, flag, 1);
#else
    (void)flag;

#ifdef USE_N25Q128
    n25q128_read_data_bytes(&n25q128, HELPER_FLAG_N25Q128_POS, flag, 1);
#endif /* USE_N25Q128 */

#endif /* USE_EEPROM */

    return (flag[0] == 1);
}

static inline void _reconstruction(void)
{
    // TODO: Same like in the _enrollment.
    uint8_t *ref_mes = (uint8_t *)&puf_sram_seed;

    puf_sram_generate_secret(ref_mes);

    _print_buf(puf_sram_id, sizeof(puf_sram_id), "ID:");
    _print_buf(codeoffset_debug, 6, "CODEOFFSET:");
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

    puts("Helper data generated");

#ifdef USE_EEPROM
    eeprom_write(PUF_SRAM_HELPER_EEPROM_START, helper, PUF_SRAM_HELPER_LEN);
    puts("Helper data written into EEPROM");
#else

#ifdef USE_N25Q128
    n25q128_page_program(&n25q128, HELPER_N25Q128_START, helper, PUF_SRAM_HELPER_LEN);
    puts("Helper data written into N25Q128");
#endif /* USE_N25Q128 */

#endif /* USE_EEPROM */
}

static int cmd_flag(int argc, char **argv)
{
    if (argc < 2 || argc > 2) {
        puts("usage: flag {set|clear|show}");
        return 1;
    }

    if (strcmp(argv[1], "set") == 0) {
        _set_flag();
    } else if (strcmp(argv[1], "clear") == 0) {
        _clear_flag();
    } else if (strcmp(argv[1], "show") == 0) {
        printf("Flag enabled? %s\n", ((_is_flag_set() == true) ? "True" : "False"));
    } else {
        puts("usage: flag {set|clear|show}");
        return 1;
    }

    return 0;
}

static int cmd_show_helper_data(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    static uint8_t buf[PUF_SRAM_HELPER_LEN] = {0};

#ifdef USE_N25Q128
    n25q128_read_data_bytes(&n25q128, HELPER_N25Q128_START, buf, PUF_SRAM_HELPER_LEN);
    _print_buf(buf, PUF_SRAM_HELPER_LEN, "show_helper_data:");
#else
    (void)buf;
#endif

    return 0;
}

static int cmd_bulk_erase(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    puts("Bulk erase the whole n25q128 flash memory. This takes about ~250 seconds!");

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

static int cmd_enroll(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!_is_flag_set()) {
        struct tm time = {
            .tm_year = 2020 - 1900,   /* years are counted from 1900 */
            .tm_mon  = 1,             /* 0 = January, 11 = December */
            .tm_mday = 1,
            .tm_hour = 1,
            .tm_min  = 1,
            .tm_sec  = 1
        };
        puts("Enable flag for next power cycle helper generation");
        _set_flag();
        puts("Enable alarm for the RTC to wake up from standby");
        rtc_set_time(&time);
        inc_secs(&time, 2);
        rtc_set_alarm(&time, 0, 0);
        puts("Going into standby now");
        pm_set(0);
    }

    return 0;
}

static int cmd_recon(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    _reconstruction();

    return 0;
}

static const shell_command_t shell_commands[] = {
    { "flag", "[set|clear|show] the helper gen status flag", cmd_flag },
    { "show", "Shows helper data", cmd_show_helper_data },
    { "be", "Erase the whole memory (n25q128). Takes ~250 seconds!", cmd_bulk_erase },
    { "se", "Erase the given sector (n25q128)", cmd_sector_erase },
    { "enroll", "Trigger the enrollment process", cmd_enroll },
    { "recon", "Trigger the reconstruction process", cmd_recon },
    { NULL, NULL, NULL }
};

int main(void)
{
    xtimer_sleep(2);

    puts("Application: puf_sram_node");

#ifdef USE_N25Q128
    n25q128.conf.bus = EXTFLASH_SPI;
    n25q128.conf.mode = SPI_MODE_0;
    n25q128.conf.clk = SPI_CLK_100KHZ;
    n25q128.conf.cs = EXTFLASH_CS;
    n25q128.conf.write = EXTFLASH_WRITE;
    n25q128.conf.hold = EXTFLASH_HOLD;
    printf("Init N25Q128 -> %s\n", ((n25q128_init(&n25q128) == 0) ? "OK" : "FAIL"));
#endif

    /* */
    if (_is_flag_set()) {
        puts("Previous power cycle flag detected");
        _enrollment();
        _clear_flag();
    }

/*
#ifdef PUF_SRAM_GEN_HELPER
    puts("PUF_SRAM_GEN_HELPER");
    if (_is_flag_set() == true) {
        puts("Previous set helper gen flag detected.");
        _enrollment();
        _clear_flag();
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
*/

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
