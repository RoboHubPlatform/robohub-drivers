#!/bin/sh
# Build and run the driver host tests.
#
# The code under test is in this repository; robohub-common (the interfaces and
# the HAL) is named by the environment, because a path into another repository
# belongs to the machine and not to this one.
#
#     ROBOHUB_COMMON_DIR=/path/to/robohub-common tests/run.sh
set -eu

: "${ROBOHUB_COMMON_DIR:?set ROBOHUB_COMMON_DIR to the robohub-common checkout}"

HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(dirname "$HERE")
OUT="${TMPDIR:-/tmp}/robohub-drivers-tests"
mkdir -p "$OUT"
CC=${CC:-cc}

# The generated namespace a build produces for cross-module includes
# (robohub/drivers/*), made here so the sources compile as they are written.
GEN="$OUT/include"
mkdir -p "$GEN/robohub/drivers/input" "$GEN/robohub/drivers/output" "$GEN/robohub/drivers/light"
D="$ROOT/drivers"
cp "$D/input/button-gpio/inc/button_gpio.h"            "$GEN/robohub/drivers/input/"
cp "$D/output/tone-pwm/inc/tone_pwm.h"                 "$GEN/robohub/drivers/output/"
cp "$D/output/rgb-pwm/inc/rgb_pwm.h"                   "$GEN/robohub/drivers/output/"
cp "$D/output/infrared-pwm/inc/infrared_pwm.h"         "$GEN/robohub/drivers/output/"
cp "$D/light/photoresistor-adc/inc/photoresistor_adc.h" "$GEN/robohub/drivers/light/"

BASE="-I$ROBOHUB_COMMON_DIR/include -I$ROBOHUB_COMMON_DIR/include/robohub \
      -I$ROBOHUB_COMMON_DIR/include/robohub/hal -I$ROBOHUB_COMMON_DIR/include/robohub/drivers \
      -I$ROBOHUB_COMMON_DIR/include/robohub/devices"

$CC -std=c11 -Wall -Wextra -I"$GEN" \
    -I"$D/input/button-gpio/inc" -I"$D/output/tone-pwm/inc" -I"$D/output/rgb-pwm/inc" \
    -I"$D/output/infrared-pwm/inc" -I"$D/light/photoresistor-adc/inc" $BASE \
    "$ROOT/tests/test_parts.c" \
    "$D/input/button-gpio/src/button_gpio.c" "$D/output/tone-pwm/src/tone_pwm.c" \
    "$D/output/rgb-pwm/src/rgb_pwm.c" "$D/output/infrared-pwm/src/infrared_pwm.c" \
    "$D/light/photoresistor-adc/src/photoresistor_adc.c" -o "$OUT/test_parts"

printf '\n== test_parts ==\n'
"$OUT/test_parts"
