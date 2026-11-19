---
id: ADR-0001
title: A driver is a value the caller owns, presenting common interfaces over the common HAL
status: proposed
date: 2026-11-19
requirements: [DRV-0001, DRV-ENV-0001, DRV-ENV-0002]
supersedes: []
superseded_by: []
---

# A driver is a value the caller owns, presenting common interfaces over the common HAL

## Context

The same parts — a DS18B20, a BMP180, a KY board's contact — are used on an
RP2350, an STM32, an ESP32 and in host tests. Programs want "a temperature
sensor", not a part number, and a part such as the BMP180 is three sensors at
once. Allocation is unavailable or unwelcome on the smallest boards.

## Decision

- A driver is written against robohub-common's HAL only — `RoboHubBus` (I2C or
  SPI alike), GPIO, ADC, PWM, timer and the single-wire bus — and contains no
  platform code. The platform implements the HAL.
- A driver presents one or more of robohub-common's driver interfaces
  (`RoboHubTemperature`, `RoboHubPressure`, `RoboHubSwitch`, …), each as a member
  of the device struct whose `context` points back at the device; the first
  member is the primary interface.
- The device is a value the caller declares and passes to `*_init`; there is no
  allocation and nothing to free.
- A driver lists the interfaces it presents in its manifest's `functions:`, from
  the vocabulary the collection's manifest declares.

## Consequences

- One object file serves every platform, and the host tests run the drivers
  over fake HALs.
- A part presenting several interfaces is read once per interface; a
  driver-specific call (`robohub_bmp180_read`) reads all of them in one pass.
- Timing-critical wires (DHT, 1-Wire) put their pulse timing in the platform's
  HAL, not in the driver.
