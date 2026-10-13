/*
 * bsp.h — Support package for the DHT11 sensor module.
 *
 * The module's own signals and the peripheral defaults it is used with. It is
 * the driver IC's counterpart of a platform board: where a platform board says
 * which MCU pin a peripheral's pins reach, this says which single-wire line the
 * module's data line is on and how the part is paced, so an application fits the
 * module without repeating those facts.
 *
 * This is the registry-specific layer of the driver stack: it resolves the named
 * line and timer from the platform's registry and hands them to the driver. The
 * MCU pin the wire lands on belongs to the platform board, so the same module is
 * fitted to a Pico, an STM32 or an ESP32.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef BOARDS_DHT11_MODULE_BSP_BSP_H
#define BOARDS_DHT11_MODULE_BSP_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "dht11.h"
#include "robohub/hal/dht.h"

/* -------------------------------------------------------------------------- */
/*  Board identification                                                      */
/* -------------------------------------------------------------------------- */
#define RH_BOARD_DHT11_MODULE 1

/* -------------------------------------------------------------------------- */
/*  Module signals                                                            */
/* -------------------------------------------------------------------------- */
/* The three wires the carrier exposes, as they are silkscreened. They are
 * connector positions, not GPIO numbers: which GPIO the data line reaches is
 * chosen when the module is wired to a platform board. */
#define RH_DHT11_MODULE_PIN_VCC  1
#define RH_DHT11_MODULE_PIN_DATA 2
#define RH_DHT11_MODULE_PIN_GND  3

/* -------------------------------------------------------------------------- */
/*  Peripheral defaults                                                       */
/* -------------------------------------------------------------------------- */
/* The names a platform registers its DHT line and timer under, and how often
 * the part may be asked. A platform that names them differently passes its own
 * to robohub_dht11_module_init. */
#define RH_DHT11_MODULE_DHT_NAME          "dht0"
#define RH_DHT11_MODULE_TIMER_NAME        "system_timer"
#define RH_DHT11_MODULE_MIN_INTERVAL_MS   ROBOHUB_DHT11_MIN_INTERVAL_MS

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
    /** Registry name of the single-wire line. Empty means "dht0". */
    rh_name dht;
    /** Registry name of the timer. Empty means "system_timer". */
    rh_name timer;
    /** Minimum interval between measurements, in milliseconds. Zero means 1000. */
    rh_u32  min_interval_ms;
} RoboHubDHT11ModuleConfig;

/* -------------------------------------------------------------------------- */
/*  Board functions                                                           */
/* -------------------------------------------------------------------------- */

/** The configuration the module is used with when a program says nothing. */
const RoboHubDHT11ModuleConfig* robohub_dht11_module_default_config(void);

/**
 * @brief Bring the module up.
 *
 * There is nothing on the carrier itself to initialise — the part is powered
 * from the module's VCC and has no reset line — but the hook exists so that
 * fitting a module reads the same as bringing a board up.
 */
void robohub_dht11_module_bsp_init(void);

/**
 * @brief Bring the module's sensor up on the named line and timer.
 *
 * Looks the two up in the registry — which is the point: the program supplies
 * the names, the platform supplied the implementations, and the board joins
 * them — then initialises the DHT11 behind its temperature and humidity
 * interfaces. Returns `ROBOHUB_ERR_NOT_FOUND` when the named line or timer is
 * not registered, which is the failure a program acts on.
 */
RoboHubError robohub_dht11_module_init(RoboHubDHT11* dev,
                                       const RoboHubDHT11ModuleConfig* config);

/* -------------------------------------------------------------------------- */
/*  Module convenience                                                        */
/* -------------------------------------------------------------------------- */
/* A breakout carries one sensor, so the common case is a program that wants a
 * reading and not a device to own. These keep the device in the board, which is
 * also what makes the module reachable from a language that has no C structs:
 * the whole of what a caller does is open, read, sleep. */
#define RH_DHT11_MODULE_ERROR (-1000000)

/** Open the module's sensor with its defaults. */
RoboHubError robohub_dht11_module_open(void);

/** Close the module's sensor. */
RoboHubError robohub_dht11_module_close(void);

/** Read the open sensor's temperature and humidity. Either may be NULL. */
RoboHubError robohub_dht11_module_read(rh_f32* celsius, rh_f32* percent);

/** The temperature in thousandths of a degree Celsius, or the error sentinel. */
rh_i32 robohub_dht11_module_milli_celsius(void);

/** The relative humidity in thousandths of a percent, or the error sentinel. */
rh_i32 robohub_dht11_module_milli_percent(void);

/** Wait, using the registered timer. */
void robohub_dht11_module_sleep_ms(rh_u32 ms);

#ifdef __cplusplus
}
#endif

#endif /* BOARDS_DHT11_MODULE_BSP_BSP_H */
