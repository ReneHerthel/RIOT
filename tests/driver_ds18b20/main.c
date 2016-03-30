/*
 * Copyright (C) 2016 Hamburg University of Applied Sciences
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       Test application for the DS18B20 temperature sensor driver
 *
 * @author      René Herthel <rene.herthel@haw-hamburg.de>
 *
 * @}
 */

#include <stdio.h>

#include "ds18b20.h"

int main(void)
{
    printf("Test application for the DS18B20 temperature sensor driver\n");

    ds18b20_test();

    return 0;
}
