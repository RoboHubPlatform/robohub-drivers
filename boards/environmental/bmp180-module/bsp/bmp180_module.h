/*
 * bsp.h — Support package for the BMP180 sensor module.
 *
 * The module's own signals and the peripheral defaults it is used with. It is
 * the driver IC's counterpart of a platform board: where a platform board says
 * which MCU pin a peripheral's pins reach, this says which bus the module is on
 * and how the part is configured, so an application fits the module without
 * repeating those facts.
 *
 * This is the registry-specific layer of the driver stack. It resolves the named
 * controller from the platform's registry, binds it as the common register bus,
 * and hands that bus to the driver — which never learns whether the wires are
 * I2C or SPI. A program supplies the names; the platform supplied the
 * implementations; this joins them.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef BOARDS_BMP180_MODULE_BSP_BSP_H
#define BOARDS_BMP180_MODULE_BSP_BSP_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "bmp180.h"
#include "robohub/hal/bus.h"

/* -------------------------------------------------------------------------- */
/*  Board identification                                                      */
/* -------------------------------------------------------------------------- */
#define RH_BOARD_BMP180_MODULE 1

/* -------------------------------------------------------------------------- */
/*  Module signals                                                            */
/* -------------------------------------------------------------------------- */
/* The wires the carrier exposes, as they are silkscreened. They are connector
 * positions, not GPIO numbers: which GPIO a wire reaches is chosen when the
 * module is wired to a platform board. The module carries both an I2C and an
 * SPI connector; a board uses whichever it fitted. */
#define RH_BMP180_MODULE_PIN_VCC 1
#define RH_BMP180_MODULE_PIN_GND 2
#define RH_BMP180_MODULE_PIN_SDA 3
#define RH_BMP180_MODULE_PIN_SCL 4
#define RH_BMP180_MODULE_PIN_SDO 5
#define RH_BMP180_MODULE_PIN_CSB 6

/* -------------------------------------------------------------------------- */
/*  Peripheral defaults                                                       */
/* -------------------------------------------------------------------------- */
/* The names a platform registers its controllers and timer under, and the
 * settings the part is used with. A platform that names them differently passes
 * its own to robohub_bmp180_module_init. */
#define RH_BMP180_MODULE_I2C_NAME    "i2c0"
#define RH_BMP180_MODULE_SPI_NAME    "spi0"
#define RH_BMP180_MODULE_TIMER_NAME  "system_timer"
#define RH_BMP180_MODULE_I2C_ADDRESS 0x77
#define RH_BMP180_MODULE_SPEED_HZ    400000u
#define RH_BMP180_MODULE_OSS         0
#define RH_BMP180_MODULE_SEA_LEVEL_PA 101325.0f

/* -------------------------------------------------------------------------- */
/*  Configuration                                                             */
/* -------------------------------------------------------------------------- */

/** Which wire the module is fitted on. I2C is the default. */
typedef enum {
    RH_BMP180_MODULE_TRANSPORT_I2C = 0,
    RH_BMP180_MODULE_TRANSPORT_SPI = 1
} RoboHubBMP180ModuleTransport;

/**
 * @brief What a program says about the module it fitted.
 *
 * Every field has a working default, so fitting the module the ordinary way —
 * on I2C, at the part's own address — is an empty configuration.
 */
typedef struct {
    /** Which wire the module is on. Zero means I2C. */
    RoboHubBMP180ModuleTransport transport;
    /** Registry name of the I2C controller. Empty means "i2c0". */
    rh_name i2c;
    /** The part's 7-bit I2C address. Zero means 0x77. */
    rh_u8   address;
    /** Registry name of the SPI controller. Empty means "spi0". */
    rh_name spi;
    /** Registry name of the GPIO controller that drives SPI chip select, when a
     *  separate pin is used. Empty means the controller drives it itself. */
    rh_name cs;
    /** Chip-select pin on that controller. */
    rh_u32  cs_pin;
    /** SPI read bit in the command byte. Zero means 0x80. */
    rh_u8   read_flag;
    /** Registry name of the timer. Empty means "system_timer". */
    rh_name timer;
    /** Pressure oversampling, 0 to 3. Zero means 0. */
    rh_u8   oversampling;
    /** Sea-level reference for altitude, in pascals. Zero means 101325. */
    rh_f32  sea_level_pa;
    /** Bus speed in hertz. Zero means 400000. */
    rh_u32  speed_hz;
} RoboHubBMP180ModuleConfig;

/* -------------------------------------------------------------------------- */
/*  Board functions                                                           */
/* -------------------------------------------------------------------------- */

/** The configuration the module is used with when a program says nothing. */
const RoboHubBMP180ModuleConfig* robohub_bmp180_module_default_config(void);

/**
 * @brief Bring the module up.
 *
 * There is nothing on the carrier itself to initialise — the part is powered
 * from the module's VCC and has no reset line — but the hook exists so that
 * fitting a module reads the same as bringing a board up.
 */
void robohub_bmp180_module_bsp_init(void);

/**
 * @brief Bring the module's sensor up on the named bus and timer.
 *
 * Looks the controller and timer up in the registry, binds the controller as the
 * common register bus, and initialises the BMP180 behind its three interfaces.
 * Returns `ROBOHUB_ERR_NOT_FOUND` when a named peripheral is not registered,
 * which is the failure a program acts on: the platform did not bring that
 * peripheral up.
 *
 * The module carries one sensor, so the bound bus is kept in the board and the
 * device this fills is meant to be that one.
 */
RoboHubError robohub_bmp180_module_init(RoboHubBMP180* dev,
                                        const RoboHubBMP180ModuleConfig* config);

/* -------------------------------------------------------------------------- */
/*  Module convenience                                                        */
/* -------------------------------------------------------------------------- */
/* A breakout carries one sensor, so the common case is a program that wants a
 * reading and not a device to own. These keep the device in the board, which is
 * also what makes the module reachable from a language that has no C structs:
 * the whole of what a caller does is open, read, sleep. */
#define RH_BMP180_MODULE_ERROR (-1000000)

/** Open the module's sensor with its defaults. */
RoboHubError robohub_bmp180_module_open(void);

/** Close the module's sensor. */
RoboHubError robohub_bmp180_module_close(void);

/** Read the open sensor's temperature, pressure and altitude. Any may be NULL. */
RoboHubError robohub_bmp180_module_read(rh_f32* celsius, rh_f32* pascals, rh_f32* metres);

/** The temperature in thousandths of a degree Celsius, or the error sentinel. */
rh_i32 robohub_bmp180_module_milli_celsius(void);

/** The pressure in pascals, or the error sentinel. */
rh_i32 robohub_bmp180_module_pascals(void);

/** The altitude in centimetres, or the error sentinel. */
rh_i32 robohub_bmp180_module_centi_metres(void);

/** Wait, using the registered timer. */
void robohub_bmp180_module_sleep_ms(rh_u32 ms);

#ifdef __cplusplus
}
#endif

#endif /* BOARDS_BMP180_MODULE_BSP_BSP_H */
