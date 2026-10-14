/*
 * bsp.c — Support package for the MAX485 RS485 transceiver module.
 *
 * The whole of it is the join: the registry holds the platform's UART and the
 * pin controller, robohub-common's adapter knows how to drive a transceiver, and
 * this looks the first two up, binds them into the third, and publishes the bus
 * so a protocol can open it by name. It owns the one bus value the caller
 * supplied and nothing else.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "bsp.h"
#include "registry.h"

#include <stddef.h>

/* -------------------------------------------------------------------------- */
/*  Defaults                                                                  */
/* -------------------------------------------------------------------------- */

static const RoboHubMax485Config max485_default = {
    .name = RH_MAX485_MODULE_NAME,
    .uart = RH_MAX485_MODULE_UART_NAME,
    .de_gpio = RH_MAX485_MODULE_DE_GPIO_NAME,
    .de_pin = RH_MAX485_MODULE_DE_PIN,
    .de_active_high = RH_TRUE,
    .timer = RH_MAX485_MODULE_TIMER_NAME,
    .baud = RH_MAX485_MODULE_BAUD,
};

const RoboHubMax485Config* robohub_max485_default_config(void)
{
    return &max485_default;
}

/* -------------------------------------------------------------------------- */
/*  Board init                                                                */
/* -------------------------------------------------------------------------- */

void robohub_max485_bsp_init(void)
{
    /* Nothing on the carrier itself to bring up: the transceiver is powered from
     * the module's VCC and has no reset. The hook exists so that fitting a module
     * reads the same as bringing a board up. */
}

/* -------------------------------------------------------------------------- */
/*  Module init                                                               */
/* -------------------------------------------------------------------------- */

RoboHubError robohub_max485_init(RoboHubRS485Uart* bus, const RoboHubMax485Config* config)
{
    if (bus == NULL) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    if (config == NULL) {
        config = robohub_max485_default_config();
    }

    rh_name name = (config->name != NULL && config->name[0] != '\0')
        ? config->name : RH_MAX485_MODULE_NAME;
    rh_name uart_name = (config->uart != NULL && config->uart[0] != '\0')
        ? config->uart : RH_MAX485_MODULE_UART_NAME;
    rh_name timer_name = (config->timer != NULL && config->timer[0] != '\0')
        ? config->timer : RH_MAX485_MODULE_TIMER_NAME;

    RoboHubUART* uart = robohub_get_uart(uart_name);
    if (uart == NULL) {
        return ROBOHUB_ERR_NOT_FOUND;
    }

    /* The timer is used for the inter-character wait and for the timed read. A
     * board without one still gets a bus; it just cannot pace the line. */
    RoboHubTimer* timer = robohub_get_timer(timer_name);

    RoboHubGPIO* de = NULL;
    if (config->de_gpio != NULL && config->de_gpio[0] != '\0') {
        de = robohub_get_gpio(config->de_gpio);
        if (de == NULL) {
            return ROBOHUB_ERR_NOT_FOUND;
        }
    }

    RoboHubError err = robohub_rs485_uart(bus, name, uart, de, config->de_pin,
                                          config->de_active_high, timer);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    rh_u32 baud = config->baud != 0u ? config->baud : RH_MAX485_MODULE_BAUD;
    err = bus->interface.init(&bus->interface, baud);
    if (ROBOHUB_FAILED(err)) {
        return err;
    }

    return robohub_register_rs485(name, &bus->interface);
}
