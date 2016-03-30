/*
 * Copyright 2016 Hamburg University of Applied Sciences
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    driver_ds18b20 DS18B20
 * @ingroup     drivers
 * @brief       Driver for the DS18B20 temperature sensor
 * @{
 *
 * @file
 * @brief       Interface definition for the DS18B20 driver
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 */

 #ifndef DS18B20_H
 #define DS18B20_H

 #include <stdint.h>

 #include "mutex.h"
 #include "periph/spi.h"
 #include "periph/gpio.h"

 #ifdef __cplusplus
 extern "C" {
 #endif

 /**
  * TODO:
  */
void ds18b20_test(void);

 #ifdef __cplusplus
 }
 #endif

 #endif /* ENC28J60_H */
 /** @} */
