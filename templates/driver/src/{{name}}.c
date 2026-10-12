/**
 * @file {{name}}.c
 * @brief {{description}}
 *
 * @project RoboHub Platform
 * @license MIT
 */

#include "{{name}}.h"

RoboHubError robohub_{{name}}_init(RoboHub{{category}}Device* self, const RoboHub{{category}}Config* config)
{
    if (self == 0 || config == 0) {
        return ROBOHUB_ERR_INVALID_ARG;
    }
    /* The bus and the timer come from the config the application filled in. */
    return ROBOHUB_OK;
}
