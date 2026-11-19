#!/usr/bin/env python3
"""Every board manifest carries the keys the board contract requires.

The contract is stated once, in the root module.yml under `contracts.board`
(`requires`, `optional`), and in the README's "Board module contract". This
reads that block and checks every boards/<category>/<name>/module.yml against
it, so a board cannot drift from what Mesh reads:

- each key under `requires` is present and not empty;
- the folder is the index: module.name is <name>, board.category is
  <category>, module.type is driver-board (DRV-BRD-0001);
- the board exports a header and a CMake target (DRV-BRD-0002);
- each signal names its id, direction, unit and field (DRV-BRD-0003);
- each connector has a name and a signal, and the block is spelled
  `connectors` (DRV-BRD-0004);
- `board.driver` names the driver the board depends on, and that driver is a
  module under drivers/; a board that depends on no driver — a transceiver
  carrier whose support package binds a HAL interface itself — names none
  (DRV-BRD-0004);
- `capabilities`, which is optional on a board, is well formed when present:
  `sensor` is a boolean and a sensor names what it measures.

Exit 0 when every board holds, 1 with every failure listed.

    python3 tests/board_manifests_test.py [repository root]
"""

# @verifies DRV-BRD-0001, DRV-BRD-0002, DRV-BRD-0003, DRV-BRD-0004

import glob
import os
import sys

try:
    import yaml
except ImportError:  # pragma: no cover - the environment, not the boards
    sys.stderr.write("board_manifests_test: PyYAML is required (pip install pyyaml)\n")
    sys.exit(2)


def load(path):
    with open(path, encoding="utf-8") as f:
        return yaml.safe_load(f) or {}


def lookup(doc, dotted):
    node = doc
    for part in dotted.split("."):
        if not isinstance(node, dict) or part not in node:
            return None
        node = node[part]
    return node


def empty(value):
    return value is None or value == "" or value == [] or value == {}


def required_keys(contract):
    """The `requires` block as dotted paths: {section: [key, ...]}."""
    keys = []
    for section, names in (contract.get("requires") or {}).items():
        for name in names or []:
            keys.append(section + "." + name)
    return keys


def driver_names(root):
    names = set()
    for path in glob.glob(os.path.join(root, "drivers", "*", "*", "module.yml")):
        name = lookup(load(path), "module.name")
        if name:
            names.add(name)
    return names


def check_board(path, root, required, drivers):
    errors = []
    doc = load(path)
    rel = os.path.relpath(path, root)
    folder = os.path.basename(os.path.dirname(path))
    category = os.path.basename(os.path.dirname(os.path.dirname(path)))

    for key in required:
        if empty(lookup(doc, key)):
            errors.append("%s: %s is missing or empty" % (rel, key))

    if lookup(doc, "module.type") != "driver-board":
        errors.append("%s: module.type is %r, not driver-board" % (rel, lookup(doc, "module.type")))
    if lookup(doc, "module.name") != folder:
        errors.append("%s: module.name %r is not its folder %r" % (rel, lookup(doc, "module.name"), folder))
    if lookup(doc, "board.category") != category:
        errors.append("%s: board.category %r is not its folder %r" % (rel, lookup(doc, "board.category"), category))

    if "connector" in (doc.get("board") or {}):
        errors.append("%s: board.connector is spelled board.connectors" % rel)
    connectors = lookup(doc, "board.connectors")
    if isinstance(connectors, list):
        for i, c in enumerate(connectors):
            if not isinstance(c, dict) or empty(c.get("name")) or empty(c.get("signal")):
                errors.append("%s: board.connectors[%d] needs a name and a signal" % (rel, i))
    elif connectors is not None:
        errors.append("%s: board.connectors is not a list" % rel)

    depended = [d.get("name") for d in (doc.get("depends") or [])
                if isinstance(d, dict) and str(d.get("name", "")).startswith("robohub-driver-")]
    driver = lookup(doc, "board.driver")
    if depended:
        if empty(driver):
            errors.append("%s: board.driver is missing; the board depends on %s" % (rel, ", ".join(depended)))
        elif driver not in depended:
            errors.append("%s: board.driver %r is not a driver it depends on (%s)" % (rel, driver, ", ".join(depended)))
    elif not empty(driver):
        errors.append("%s: board.driver %r is named but not in depends" % (rel, driver))
    if not empty(driver) and driver not in drivers:
        errors.append("%s: board.driver %r is no module under drivers/" % (rel, driver))

    for key in ("exports.headers", "exports.cmake.target"):
        if empty(lookup(doc, key)):
            errors.append("%s: %s is missing" % (rel, key))

    for i, s in enumerate(doc.get("signals") or []):
        for field in ("id", "direction", "field"):
            if not isinstance(s, dict) or empty(s.get(field)):
                errors.append("%s: signals[%d] has no %s" % (rel, i, field))
        if isinstance(s, dict):
            if "unit" not in s:
                errors.append("%s: signals[%d] has no unit (empty for a unitless signal)" % (rel, i))
            if s.get("direction") not in (None, "input", "output"):
                errors.append("%s: signals[%d] direction %r is not input or output" % (rel, i, s.get("direction")))

    caps = doc.get("capabilities")
    if caps is not None:
        if not isinstance(caps, dict):
            errors.append("%s: capabilities is not a map" % rel)
        else:
            if "sensor" in caps and not isinstance(caps["sensor"], bool):
                errors.append("%s: capabilities.sensor is not a boolean" % rel)
            if caps.get("sensor") is True and empty(caps.get("measures")):
                errors.append("%s: capabilities.sensor is true but capabilities.measures is empty" % rel)
    return errors


def main(argv):
    root = os.path.abspath(argv[1] if len(argv) > 1 else os.path.join(os.path.dirname(__file__), ".."))
    contract = lookup(load(os.path.join(root, "module.yml")), "contracts.board")
    if not isinstance(contract, dict):
        print("FAIL module.yml: contracts.board is missing")
        return 1
    pattern = contract.get("discovery", "boards/*/*/module.yml")
    boards = sorted(glob.glob(os.path.join(root, pattern)))
    if not boards:
        print("FAIL no board manifests match %s" % pattern)
        return 1
    required = required_keys(contract)
    drivers = driver_names(root)
    errors = []
    for path in boards:
        errors.extend(check_board(path, root, required, drivers))
    for e in errors:
        print("FAIL " + e)
    print("%d board manifests, %d failures (required: %s)" % (len(boards), len(errors), ", ".join(required)))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
