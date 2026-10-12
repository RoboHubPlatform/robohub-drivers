/**
 * @file main.c
 * @brief {{description}}
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "platform.h"
#include "{{name}}.h"

int main(void)
{
    (void)robohub_platform_init();
    RoboHubtemperatureDevice device;
    RoboHubtemperatureConfig config = { 0 };
    (void)robohub_{{name}}_init(&device, &config);
    for (;;) {
    }
}
