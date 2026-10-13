/**
 * @file main.c
 * @brief {{description}}
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "platform.h"
#include "registry.h"
#include "robohub/hal/bus.h"
#include "{{name}}.h"

int main(void)
{
    (void)robohub_platform_init();

    /* The same three layers a program sees: the platform registers the
     * controller, the board binds it as the common bus, and the driver is
     * written against the bus. Replace the address and the controller name with
     * the ones the board registers. */
    RoboHubBusI2C bus;
    if (robohub_bus_i2c_open(&bus, "i2c0", 0x00, 0u) != ROBOHUB_OK) {
        for (;;) {
        }
    }

    RoboHub{{name}}Device device;
    RoboHub{{name}}Config config = { .bus = &bus.bus };
    if (robohub_{{name}}_init(&device, &config) != ROBOHUB_OK) {
        for (;;) {
        }
    }

    for (;;) {
        rh_f32 celsius = 0.0f;
        (void)device.interface.get_celsius(&device.interface, &celsius);
    }
}
