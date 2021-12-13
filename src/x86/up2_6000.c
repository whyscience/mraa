/*
 * Author: Kunyang Fan <kunyang_fan@aaeon.com.tw>
 * Based on work from: Dan O'Donovan <dan@emutex.com>
 *                     Nicola Lunghi <nicola.lunghi@emutex.com>
 * Copyright (c) 2021 AAEON Corporation.
 * Copyright (c) 2017 Emutex Ltd.
 * Copyright (c) 2014 Intel Corporation.
 *
 * SPDX-License-Identifier: MIT
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <unistd.h>

#include "common.h"
#include "gpio.h"
#include "x86/up2_6000.h"

#define PLATFORM_NAME "UP2_6000"
#define PLATFORM_VERSION "1.0.0"

#define MRAA_UP2_6000_GPIOCOUNT   28

#define MRAA_UP2_6000_COMMUNITY0_BASE   445
#define MRAA_UP2_6000_COMMUNITY1_BASE   332
#define MRAA_UP2_6000_COMMUNITY3_BASE   285
#define MRAA_UP2_6000_COMMUNITY4_BASE   205
#define MRAA_UP2_6000_COMMUNITY5_BASE   197

// utility function to setup pin mapping of boards
static mraa_result_t
mraa_up2_6000_set_pininfo(mraa_board_t* board, int mraa_index, char* name,
        mraa_pincapabilities_t caps, int sysfs_pin, int chip, int line)
{
    if (mraa_index < board->phy_pin_count) {
        mraa_pininfo_t* pin_info = &board->pins[mraa_index];
        strncpy(pin_info->name, name, MRAA_PIN_NAME_SIZE);
        pin_info->capabilities = caps;
        if (caps.gpio) {
            pin_info->gpio.pinmap = sysfs_pin;
            pin_info->gpio.mux_total = 0;
            pin_info->gpio.gpio_chip = chip;
            pin_info->gpio.gpio_line = line;
        }
        if (caps.pwm) {
            pin_info->pwm.parent_id = 0;
            pin_info->pwm.pinmap = 0;
            pin_info->pwm.mux_total = 0;
        }
        if (caps.aio) {
            pin_info->aio.mux_total = 0;
            pin_info->aio.pinmap = 0;
        }
        if (caps.i2c) {
            pin_info->i2c.pinmap = 1;
            pin_info->i2c.mux_total = 0;
        }
        if (caps.spi) {
            pin_info->spi.mux_total = 0;
        }
        if (caps.uart) {
            pin_info->uart.mux_total = 0;
        }
        return MRAA_SUCCESS;
    }
    return MRAA_ERROR_INVALID_RESOURCE;
}

static mraa_result_t
mraa_up2_6000_get_pin_index(mraa_board_t* board, char* name, int* pin_index)
{
    int i;
    for (i = 0; i < board->phy_pin_count; ++i) {
        if (strncmp(name, board->pins[i].name, MRAA_PIN_NAME_SIZE) == 0) {
            *pin_index = i;
            return MRAA_SUCCESS;
        }
    }

    syslog(LOG_CRIT, "up2_6000: Failed to find pin name %s", name);

    return MRAA_ERROR_INVALID_RESOURCE;
}

mraa_board_t*
mraa_up2_6000_board()
{
    mraa_board_t* b = (mraa_board_t*) calloc(1, sizeof (mraa_board_t));

    if (b == NULL) {
        return NULL;
    }

    b->platform_name = PLATFORM_NAME;
    b->platform_version = PLATFORM_VERSION;
    b->phy_pin_count = MRAA_UP2_6000_PINCOUNT;
    b->gpio_count = MRAA_UP2_6000_GPIOCOUNT;
    b->chardev_capable = 0;

    b->pins = (mraa_pininfo_t*) malloc(sizeof(mraa_pininfo_t) * MRAA_UP2_6000_PINCOUNT);
    if (b->pins == NULL) {
        goto error;
    }

    b->adv_func = (mraa_adv_func_t *) calloc(1, sizeof (mraa_adv_func_t));
    if (b->adv_func == NULL) {
        free(b->pins);
        goto error;
    }

    mraa_up2_6000_set_pininfo(b, 0, "INVALID",    (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 1, "3.3v",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 2, "5v",         (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 3, "I2C_SDA",    (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 1, 0, 0}, MRAA_UP2_6000_COMMUNITY1_BASE + 22, 1, 22);
    mraa_up2_6000_set_pininfo(b, 4, "5v",         (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 5, "I2C_SCL",    (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 1, 0, 0}, MRAA_UP2_6000_COMMUNITY1_BASE + 23, 1, 23);
    mraa_up2_6000_set_pininfo(b, 6, "GND",        (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 7, "GPIO4",      (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY4_BASE + 31, 3, 31);
    mraa_up2_6000_set_pininfo(b, 8, "UART_TX",    (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 1}, MRAA_UP2_6000_COMMUNITY4_BASE + 13, 3, 13);
    mraa_up2_6000_set_pininfo(b, 9, "GND",        (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 10, "UART_RX",   (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 1}, MRAA_UP2_6000_COMMUNITY4_BASE + 12, 3, 12);
    mraa_up2_6000_set_pininfo(b, 11, "UART_RTS",  (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 1}, MRAA_UP2_6000_COMMUNITY4_BASE + 14, 3, 14);
    mraa_up2_6000_set_pininfo(b, 12, "I2S_CLK",   (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY0_BASE + 53, 0, 53);
    mraa_up2_6000_set_pininfo(b, 13, "GPIO27",    (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY4_BASE + 11, 3, 11);
    mraa_up2_6000_set_pininfo(b, 14, "GND",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 15, "GPIO22",    (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY4_BASE + 9, 3, 9);
    mraa_up2_6000_set_pininfo(b, 16, "GPIO19",    (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY4_BASE + 78, 3, 78);
    mraa_up2_6000_set_pininfo(b, 17, "3.3v",      (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 18, "GPIO24",    (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY4_BASE + 77, 3, 77);
    mraa_up2_6000_set_pininfo(b, 19, "SPI0_MOSI", (mraa_pincapabilities_t) {1, 1, 0, 0, 1, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY0_BASE + 22, 0, 22);
    mraa_up2_6000_set_pininfo(b, 20, "GND",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 21, "SPI0_MISO", (mraa_pincapabilities_t) {1, 1, 0, 0, 1, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY0_BASE + 22, 0, 21);
    mraa_up2_6000_set_pininfo(b, 22, "GPIO25",    (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY0_BASE + 11, 0, 11);
    mraa_up2_6000_set_pininfo(b, 23, "SPI0_CLK",  (mraa_pincapabilities_t) {1, 1, 0, 0, 1, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY0_BASE + 20, 0, 20);
    mraa_up2_6000_set_pininfo(b, 24, "SPI0_CS0",  (mraa_pincapabilities_t) {1, 1, 0, 0, 1, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY0_BASE + 19, 0, 19);
    mraa_up2_6000_set_pininfo(b, 25, "GND",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 26, "SPI0_CS1",  (mraa_pincapabilities_t) {1, 1, 0, 0, 1, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY0_BASE + 23, 0, 23);
    mraa_up2_6000_set_pininfo(b, 27, "ID_SD",     (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 1, 0, 0}, MRAA_UP2_6000_COMMUNITY0_BASE + 9, 0, 9);
    mraa_up2_6000_set_pininfo(b, 28, "ID_SC",     (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 1, 0, 0}, MRAA_UP2_6000_COMMUNITY0_BASE + 9, 0, 10);
    mraa_up2_6000_set_pininfo(b, 29, "GPIO5",     (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY4_BASE + 42, 3, 42);
    mraa_up2_6000_set_pininfo(b, 30, "GND",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 31, "GPIO6",     (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY4_BASE + 43, 3, 43);
    mraa_up2_6000_set_pininfo(b, 32, "PWM0",      (mraa_pincapabilities_t) {1, 1, 1, 0, 0, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY4_BASE + 5, 3, 5);
    mraa_up2_6000_set_pininfo(b, 33, "PWM1",      (mraa_pincapabilities_t) {1, 1, 1, 0, 0, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY1_BASE + 44, 1, 44);
    mraa_up2_6000_set_pininfo(b, 34, "GND",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 35, "I2S_FRM",   (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY0_BASE + 54, 0, 54);
    mraa_up2_6000_set_pininfo(b, 36, "UART_CTS",  (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 1}, MRAA_UP2_6000_COMMUNITY4_BASE + 15, 3, 15);
    mraa_up2_6000_set_pininfo(b, 37, "GPIO26",    (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY4_BASE + 34, 3, 34);
    mraa_up2_6000_set_pininfo(b, 38, "I2S_DIN",   (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY0_BASE + 56, 0, 56);
    mraa_up2_6000_set_pininfo(b, 39, "GND",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 40, "I2S_DOUT",  (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 0}, MRAA_UP2_6000_COMMUNITY0_BASE + 55, 0, 55);
    // Sub Carrier Board
    mraa_up2_6000_set_pininfo(b, 41, "5v",        (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 42, "3.3v",      (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 43, "5v",        (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 44, "3.3v",      (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 45, "GND",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 46, "GND",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 47, "CAN0_TX",   (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 48, "CAN0_RX",   (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 49, "GND",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 50, "GND",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 51, "CAN1_TX",   (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 52, "CAN1_RX",   (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 53, "GND",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 54, "GND",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 55, "QEP_A0",    (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 56, "QEP_B0",    (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 57, "QEP_A1",    (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 58, "QEP_B1",    (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 59, "QEP_A2",    (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 60, "QEP_B2",    (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 61, "QEP_A3",    (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 62, "QEP_B3",    (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 63, "QEP_I0",    (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 64, "QEP_I2",    (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 65, "QEP_I1",    (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 66, "QEP_I3",    (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 67, "PWM4",      (mraa_pincapabilities_t) {1, 0, 1, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 68, "PWM2",      (mraa_pincapabilities_t) {1, 0, 1, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 69, "PWM5",      (mraa_pincapabilities_t) {1, 0, 1, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 70, "PWM3",      (mraa_pincapabilities_t) {1, 0, 1, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 71, "GND",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 72, "GND",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 73, "ADC0",      (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 74, "ADC2",      (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 75, "ADC1",      (mraa_pincapabilities_t) {0, 1, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 76, "ADC3",      (mraa_pincapabilities_t) {1, 1, 0, 0, 0, 0, 0, 1}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 77, "GND",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 78, "GND",       (mraa_pincapabilities_t) {0, 0, 0, 0, 0, 0, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 79, "I2C2_SDA",  (mraa_pincapabilities_t) {1, 0, 0, 0, 0, 1, 0, 0}, -1, -1, -1);
    mraa_up2_6000_set_pininfo(b, 80, "I2C2_SCL",  (mraa_pincapabilities_t) {1, 0, 0, 0, 0, 1, 0, 0}, -1, -1, -1);
 
    b->i2c_bus_count = 0;
    b->def_i2c_bus = 0;
    int i2c_bus_num;

    // Configure I2C adaptor #0 (default)
    // (For consistency with Raspberry Pi 2, use I2C1 as our primary I2C bus)
    i2c_bus_num = mraa_find_i2c_bus_pci("0000:00", "0000:00:15.3", "i2c_designware.4");
    if (i2c_bus_num != -1) {
        int i = b->i2c_bus_count;
        b->i2c_bus[i].bus_id = i2c_bus_num;
        mraa_up2_6000_get_pin_index(b, "I2C_SDA", &(b->i2c_bus[i].sda));
        mraa_up2_6000_get_pin_index(b, "I2C_SCL", &(b->i2c_bus[i].scl));
        b->i2c_bus_count++;
    }

    // Configure I2C adaptor #1
    // (normally reserved for accessing HAT EEPROM)
    i2c_bus_num = mraa_find_i2c_bus_pci("0000:00", "0000:00:19.1", "i2c_designware.6");
    if (i2c_bus_num != -1) {
        int i = b->i2c_bus_count;
        b->i2c_bus[i].bus_id = i2c_bus_num;
        mraa_up2_6000_get_pin_index(b, "ID_SD", &(b->i2c_bus[i].sda));
        mraa_up2_6000_get_pin_index(b, "ID_SC", &(b->i2c_bus[i].scl));
        b->i2c_bus_count++;
    }

    // Configure I2C adaptor #2
    // (normally reserved for accessing HAT EEPROM)
    i2c_bus_num = mraa_find_i2c_bus_pci("0000:00", "0000:00:15.2", "i2c_designware.3");
    if (i2c_bus_num != -1) {
        int i = b->i2c_bus_count;
        b->i2c_bus[i].bus_id = i2c_bus_num;
        mraa_up2_6000_get_pin_index(b, "I2C2_SDA", &(b->i2c_bus[i].sda));
        mraa_up2_6000_get_pin_index(b, "I2C2_SCL", &(b->i2c_bus[i].scl));
        b->i2c_bus_count++;
    }

    // Configure PWM
    b->pwm_dev_count = 0;
    b->def_pwm_dev = 0;
    b->pwm_default_period = 5000;
    b->pwm_max_period = 218453;
    b->pwm_min_period = 1;

    // set the correct pwm channels for pwm 1 2
    b->pins[32].pwm.parent_id = 0;
    b->pins[32].pwm.pinmap = 1;
    b->pwm_dev_count++;
    b->pins[33].pwm.parent_id = 0;
    b->pins[33].pwm.pinmap = 2;
    b->pwm_dev_count++;
    b->pins[68].pwm.parent_id = 0;
    b->pins[68].pwm.pinmap = 3;
    b->pwm_dev_count++;
    b->pins[70].pwm.parent_id = 0;
    b->pins[70].pwm.pinmap = 4;
    b->pwm_dev_count++;
    b->pins[66].pwm.parent_id = 0;
    b->pins[66].pwm.pinmap = 5;
    b->pwm_dev_count++;
    b->pins[69].pwm.parent_id = 0;
    b->pins[69].pwm.pinmap = 6;
    b->pwm_dev_count++;

    // Configure SPI
    b->spi_bus_count = 0;
    b->def_spi_bus = 0;

    // Configure SPI #0 CS0 (default)
    b->spi_bus[0].bus_id = 2;
    b->spi_bus[0].slave_s = 0;
    mraa_up2_6000_get_pin_index(b, "SPI0_CS0",  &(b->spi_bus[0].cs));
    mraa_up2_6000_get_pin_index(b, "SPI0_MOSI", &(b->spi_bus[0].mosi));
    mraa_up2_6000_get_pin_index(b, "SPI0_MISO", &(b->spi_bus[0].miso));
    mraa_up2_6000_get_pin_index(b, "SPI0_CLK",  &(b->spi_bus[0].sclk));
    b->spi_bus_count++;

    // Configure SPI #0 CS1
    b->spi_bus[1].bus_id = 1;
    b->spi_bus[1].slave_s = 1;
    mraa_up2_6000_get_pin_index(b, "SPI0_CS1",  &(b->spi_bus[1].cs));
    mraa_up2_6000_get_pin_index(b, "SPI0_MOSI", &(b->spi_bus[1].mosi));
    mraa_up2_6000_get_pin_index(b, "SPI0_MISO", &(b->spi_bus[1].miso));
    mraa_up2_6000_get_pin_index(b, "SPI0_CLK",  &(b->spi_bus[1].sclk));
    b->spi_bus_count++;

    // Configure UART
    b->uart_dev_count = 0;
    b->def_uart_dev = 0;
    // setting up a default path
    if (mraa_find_uart_bus_pci("/sys/bus/pci/devices/0000:00:1e.1/dw-apb-uart.9/tty/",
                               &(b->uart_dev[0].device_path)) != MRAA_SUCCESS) {
        syslog(LOG_WARNING, "UP2 6000: Platform failed to find uart controller");
    } else {
        // Configure UART #1 (default)
        mraa_up2_6000_get_pin_index(b, "UART_RX", &(b->uart_dev[0].rx));
        mraa_up2_6000_get_pin_index(b, "UART_TX", &(b->uart_dev[0].tx));
        mraa_up2_6000_get_pin_index(b, "UART_CTS", &(b->uart_dev[0].cts));
        mraa_up2_6000_get_pin_index(b, "UART_RTS", &(b->uart_dev[0].rts));
        b->uart_dev_count++;
    }

    // Configure ADCs
    b->aio_count = 0;

    /* We skip the check UP pinctrl driver check*/
    const char* pinctrl_path = "/sys/bus/platform/drivers/gpio-aaeon";
    int have_pinctrl = access(pinctrl_path, F_OK) != -1;
    syslog(LOG_NOTICE, "up2 6000: kernel WMI pinctrl driver %savailable", have_pinctrl ? "" : "un");

    if (1 || have_pinctrl)
        return b;

error:
    syslog(LOG_CRIT, "up2 6000: Platform failed to initialise");
    free(b);
    return NULL;
}
