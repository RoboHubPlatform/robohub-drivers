/*
 * bsp.h — Support package for the DS18B20 temperature sensor module.
 *
 * The module's own signals and the peripheral defaults it is used with. It is
 * the driver IC's counterpart of a platform board: where a platform-board says
 * which MCU pin a peripheral's pins reach, this says which 1-Wire bus the
 * module's data line is on and how the part is configured, so an application
 * fits the module without repeating those facts.
 *
 * The GPIO a wire lands on is the platform board's business and not the
 * module's. The same carrier is fitted to a Pico, an STM32 or an ESP32, and
 * only the platform knows which of its pins the module was wired to. So the
 * pins here are the module's own connector positions, and the peripheral the
 * data line reaches is named rather than numbered.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef BOARDS_DS18B20_TEMPERATURE_MODULE_BSP_BSP_H
#define BOARDS_DS18B20_TEMPERATURE_MODULE_BSP_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "ds18b20.h"

/* -------------------------------------------------------------------------- */
/*  Board identification                                                      */
/* -------------------------------------------------------------------------- */
#define RH_BOARD_DS18B20_MODULE 1

/* -------------------------------------------------------------------------- */
/*  Module signals                                                            */
/* -------------------------------------------------------------------------- */
/* The three wires the carrier exposes, as they are silkscreened. They are
 * connector positions, not GPIO numbers: which GPIO the data line reaches is
 * chosen when the module is wired to a platform board. */
#define RH_DS18B20_MODULE_PIN_VCC 1
#define RH_DS18B20_MODULE_PIN_GND 2
#define RH_DS18B20_MODULE_PIN_DQ  3

/* -------------------------------------------------------------------------- */
/*  Peripheral defaults                                                       */
/* -------------------------------------------------------------------------- */
/* The names a platform registers its 1-Wire bus and its timer under, and the
 * resolution the part is configured for. A platform that names them differently
 * passes its own to robohub_ds18b20_module_init. */
#define RH_DS18B20_MODULE_ONEWIRE_NAME "onewire0"
#define RH_DS18B20_MODULE_TIMER_NAME   "system_timer"
#define RH_DS18B20_MODULE_RESOLUTION   12

/* -------------------------------------------------------------------------- */
/*  Configuration                                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief What a program says about the module it fitted.
 *
 * Every field has a working default, so fitting the module the ordinary way is
 * an empty configuration.
 */
typedef struct {
    /** Registry name of the 1-Wire bus. Empty means "onewire0". */
    rh_name onewire;
    /** Registry name of the timer. Empty means "system_timer". */
    rh_name timer;
    /** Conversion resolution in bits, 9 to 12. Zero means 12. */
    rh_u8   resolution_bits;
    /** Upper alarm limit in whole degrees Celsius. Zero means +30. */
    rh_i8   alarm_high_c;
    /** Lower alarm limit in whole degrees Celsius. Zero means 0. */
    rh_i8   alarm_low_c;
} RoboHubDS18B20ModuleConfig;

/* -------------------------------------------------------------------------- */
/*  Board functions                                                           */
/* -------------------------------------------------------------------------- */

/** The configuration the module is used with when a program says nothing. */
const RoboHubDS18B20ModuleConfig* robohub_ds18b20_module_default_config(void);

/**
 * @brief Bring the module up.
 *
 * Called once, before the sensor is read. There is nothing on the carrier
 * itself to initialise — the part is powered from the module's VCC and has no
 * reset line — but the hook exists so that fitting a module reads the same as
 * bringing a board up, and so a carrier that does need something has a place.
 */
void robohub_ds18b20_module_bsp_init(void);

/**
 * @brief Bring the module's sensor up on the named bus and timer.
 *
 * Looks the two up in the registry — which is the point: the program supplies
 * the names, the platform supplied the implementations, and the board joins
 * them — then initialises the DS18B20 behind the temperature interface.
 * Returns `ROBOHUB_ERR_NOT_FOUND` when the named bus or timer is not
 * registered, which is the failure a program acts on: the platform did not
 * bring that peripheral up.
 */
RoboHubError robohub_ds18b20_module_init(RoboHubDS18B20* dev,
                                         const RoboHubDS18B20ModuleConfig* config);

/* -------------------------------------------------------------------------- */
/*  Module convenience                                                        */
/* -------------------------------------------------------------------------- */
/* A breakout carries one sensor, so the common case is a program that wants a
 * reading and not a device to own. These keep the device in the board, which is
 * also what makes the module reachable from a language that has no C structs:
 * the whole of what a caller does is open, read, sleep. */
#define RH_DS18B20_MODULE_TEMP_ERROR (-1000000)

/** Open the module's sensor with its defaults. */
RoboHubError robohub_ds18b20_module_open(void);

/** Close the module's sensor. */
RoboHubError robohub_ds18b20_module_close(void);

/** Read the open sensor in degrees Celsius. */
RoboHubError robohub_ds18b20_module_read(rh_f32* celsius);

/** The same reading as a float, or -1000.0 on failure. */
rh_f32 robohub_ds18b20_module_celsius(void);

/**
 * @brief The same reading in thousandths of a degree Celsius.
 *
 * `RH_DS18B20_MODULE_TEMP_ERROR` on failure. This is the reading a language
 * without a float type at the foreign boundary can take: it is an integer on
 * both sides, and the caller formats it. */
rh_i32 robohub_ds18b20_module_milli_celsius(void);

/** Wait, using the registered timer. */
void robohub_ds18b20_module_sleep_ms(rh_u32 ms);

#ifdef __cplusplus
}
#endif

#endif /* BOARDS_DS18B20_TEMPERATURE_MODULE_BSP_BSP_H */
