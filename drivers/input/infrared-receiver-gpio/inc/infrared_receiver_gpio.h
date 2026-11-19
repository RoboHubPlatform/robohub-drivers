/**
 * @file infrared_receiver_gpio.h
 * @brief Driver - an infrared receiver on a GPIO, as a RoboHubInfraredReceiver.
 *
 * The other end of the infrared transmitter: a module that hears the codes a
 * remote sends. What arrives at the pin is a burst of pulses, and a protocol is
 * what gives the burst a meaning, so the driver times the pulses and decodes
 * them.
 *
 * NEC is decoded, because it is what most remotes speak and what these modules
 * are sold for: a header, then thirty-two bits whose space says whether they are
 * a zero or a one — an address byte, a second address byte, a command and the
 * command inverted. In standard NEC the second address byte is the first
 * inverted; in extended NEC, which most remotes now send, the two make a 16-bit
 * address. Both are decoded. The command's inverse is what checks a code, so a
 * frame whose command does not invert is refused.
 *
 * The code read is `(address << 8) | command`: bits 0-7 the command, bits 8-15
 * the address of a standard frame, bits 8-23 the address of an extended one. A
 * repeat code (a held button) and a burst that is not NEC are reported as none,
 * which is different from a code that was.
 *
 * The timing is taken by polling the pin, and the resolution of that poll is
 * what a remote's shortest pulse has to fit in. NEC's shortest level is about
 * 560 µs and a one's space is about 1690 µs, so a poll of tens of microseconds
 * is enough to tell them apart.
 *
 * @project RoboHub Platform
 * @license MIT
 */

#ifndef INFRARED_RECEIVER_GPIO_H
#define INFRARED_RECEIVER_GPIO_H

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "types.h"
#include "error.h"
#include "robohub/hal/gpio.h"
#include "robohub/hal/timer.h"
#include "robohub/drivers/infrared_receiver.h"

/** One infrared receiver on one pin. */
typedef struct {
    RoboHubInfraredReceiver interface; /* Must remain the first member. */
    RoboHubGPIO*  gpio;
    rh_u32        pin;
    RoboHubTimer* timer;
    rh_bool       active_low;   /* The module pulls the line low for a mark. */
    rh_u32        code;          /* The last code understood. */
    rh_bool       have_code;
    rh_u8         protocol;
    rh_bool       ready;
} RoboHubInfraredReceiverGpio;

/**
 * @brief Bind a receiver to a GPIO pin and a timer.
 *
 * The timer is what times the pulses; without one the driver cannot hear
 * anything. `active_low` is the wiring: most of these modules pull the line low
 * for a mark.
 */
RoboHubError robohub_infrared_receiver_gpio_init(RoboHubInfraredReceiverGpio* ir,
                                                 RoboHubGPIO* gpio,
                                                 rh_u32 pin,
                                                 RoboHubTimer* timer,
                                                 rh_bool active_low);

#ifdef __cplusplus
}
#endif

#endif /* INFRARED_RECEIVER_GPIO_H */
