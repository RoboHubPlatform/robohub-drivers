/**
 * @file sht4x_module.h
 * @brief Board support for an SHT4x breakout on I2C (DRV-BRD-0005, robohub
 *        ADR-0026).
 *
 * Which bus the part is on, at which address, and the one call Mesh's
 * generated firmware makes to bring it up:
 *
 *     RoboHubSHT4x sensor;
 *     robohub_sht4x_module_init(&sensor, NULL);   // i2c0, 0x44, 100 kHz
 *
 * The bus's pins are the platform's: each board registers "i2c0" on the pins
 * its spec gives (GP8/GP9 on a Pico 2 W, GPIO23/GPIO19 on a LOLIN32 Lite).
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef BOARDS_SHT4X_MODULE_BSP_H
#define BOARDS_SHT4X_MODULE_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "sht4x.h"

#define RH_BOARD_SHT4X_MODULE 1

/* The four wires the breakout brings out. */
#define RH_SHT4X_MODULE_PIN_VCC 1
#define RH_SHT4X_MODULE_PIN_GND 2
#define RH_SHT4X_MODULE_PIN_SDA 3
#define RH_SHT4X_MODULE_PIN_SCL 4

/* The names a platform registers its bus and timer under, and the part. */
#define RH_SHT4X_MODULE_I2C_NAME   "i2c0"
#define RH_SHT4X_MODULE_TIMER_NAME "system_timer"
#define RH_SHT4X_MODULE_ADDRESS    ROBOHUB_SHT4X_ADDRESS
#define RH_SHT4X_MODULE_SPEED_HZ   ROBOHUB_SHT4X_SPEED_HZ

typedef struct {
    /** Registry name of the I2C bus. Empty means "i2c0". */
    rh_name i2c;
    /** Registry name of the timer. Empty means "system_timer". */
    rh_name timer;
    /** The part's address. Zero means 0x44. */
    rh_u8   address;
    /** The bus speed. Zero means 100 kHz. */
    rh_u32  speed_hz;
} RoboHubSHT4xModuleConfig;

const RoboHubSHT4xModuleConfig* robohub_sht4x_module_default_config(void);
void robohub_sht4x_module_bsp_init(void);
/** Find the bus and the timer, and bring the part up. NULL means the defaults. */
RoboHubError robohub_sht4x_module_init(RoboHubSHT4x* dev, const RoboHubSHT4xModuleConfig* config);

#ifdef __cplusplus
}
#endif

#endif /* BOARDS_SHT4X_MODULE_BSP_H */
