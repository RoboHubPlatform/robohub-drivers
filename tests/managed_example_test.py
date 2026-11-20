#!/usr/bin/env python3
"""The DS18B20 telemetry example takes the managed layer only when provisioned
(DRV-TEMP-0005).

Builds the example twice for the Pico 2 W, in temporary copies: as it is — no
update key, so no managed layer in the image — and as Mesh would provision it,
with robohub_secret.h, robohub_update_key.h and CONFIG_RH_NODE_MANAGED=y — so
the image carries robohub_managed_init. Skipped (77) without the Pico toolchain.
"""
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
EXAMPLE = os.path.join(os.path.dirname(HERE), "examples", "temperature", "ds18b20", "c", "telemetry")
WORKSPACE = os.environ.get("ROBOHUB_WORKSPACE", os.path.dirname(os.path.dirname(HERE)))


def build(root, provisioned):
    copy = os.path.join(root, "telemetry")
    shutil.copytree(EXAMPLE, copy, ignore=shutil.ignore_patterns("build"))
    if provisioned:
        with open(os.path.join(copy, "src", "robohub_secret.h"), "w") as f:
            f.write("#pragma once\n#define RH_SECURE_PROVISIONED 1\n#define RH_DEVICE_KEY_EPOCH 0u\n"
                    "static const unsigned char RH_DEVICE_UID[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };\n"
                    "static const unsigned char RH_DEVICE_KEY[16] = { 9 };\n")
        with open(os.path.join(copy, "src", "robohub_update_key.h"), "w") as f:
            f.write("#pragma once\n#define RH_UPDATE_KEY_PROVISIONED 1\n"
                    "static const unsigned char RH_UPDATE_PUBLIC_KEY[32] = { 1 };\n")
        with open(os.path.join(copy, "project.conf"), "a") as f:
            f.write("CONFIG_RH_NODE_MANAGED=y\n")
    done = subprocess.run(["rho", "build", "-target", "pico2w", "."], cwd=copy, capture_output=True, text=True,
                          env=dict(os.environ, ROBOHUB_WORKSPACE=WORKSPACE))
    if done.returncode != 0:
        print(done.stdout[-2000:], done.stderr[-2000:])
        return None
    elf = os.path.join(copy, "build", "output", "telemetry.elf")
    return subprocess.run(["arm-none-eabi-nm", elf], capture_output=True, text=True).stdout


def main():
    if not shutil.which("arm-none-eabi-nm") or not shutil.which("rho"):
        print("skipped: the Pico toolchain (arm-none-eabi) or rho is not installed")
        return 77
    failures = []
    with tempfile.TemporaryDirectory() as plain_root, tempfile.TemporaryDirectory() as managed_root:
        plain = build(plain_root, False)
        managed = build(managed_root, True)
    if plain is None or managed is None:
        print("FAIL: the example did not build")
        return 1
    if " T robohub_managed_init" in plain:
        failures.append("an unprovisioned build carries the managed layer")
    if " T robohub_managed_init" not in managed or " T crypto_ed25519_check" not in managed:
        failures.append("a provisioned build lacks the managed layer")
    for f in failures:
        print("FAIL:", f)
    if not failures:
        print("ok: the managed layer only when provisioned")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
