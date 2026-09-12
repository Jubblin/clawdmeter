#!/usr/bin/env bash
# Drops clawdmeter_amoled_206_model.py into the installed esphome package's
# mipi_spi/models/ directory, where esphome/components/mipi_spi/display.py
# auto-discovers every module in that folder via pkgutil.iter_modules() and
# registers whatever DriverChip.extend(...) calls it makes.
#
# Why a patch script instead of an external_components override: overriding
# a core ESPHome component via `external_components:` requires shipping the
# ENTIRE component (esphome/components/mipi_spi/ is ~3100 lines across
# display.py/mipi_spi.cpp/mipi_spi.h/models/*.py as of ESPHome 2026.6.x) —
# vendoring all of that just to add one 12-line model registration would
# immediately drift from upstream. Dropping one small file into the models/
# directory (which is designed to be a flat, auto-scanned registry) needs no
# such vendoring, at the cost of having to re-run this script whenever the
# esphome venv/install is recreated. The real fix is upstreaming this model
# to esphome/esphome (esphome/components/mipi_spi/models/waveshare.py) the
# same way the 2.16 and 1.75 were added — see PR esphome/esphome#16887 for
# the precedent. Until then, run this before every `esphome compile`/`run`.
#
# Usage: esphome/patches/apply.sh [path-to-esphome-venv-python]
set -euo pipefail

PYTHON="${1:-python3}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

MODELS_DIR="$("$PYTHON" -c "import esphome, os; print(os.path.join(os.path.dirname(esphome.__file__), 'components', 'mipi_spi', 'models'))")"

if [ ! -d "$MODELS_DIR" ]; then
    echo "error: $MODELS_DIR does not exist — is mipi_spi present in this esphome install?" >&2
    exit 1
fi

cp "$SCRIPT_DIR/clawdmeter_amoled_206_model.py" "$MODELS_DIR/clawdmeter_amoled_206_model.py"
echo "Installed clawdmeter_amoled_206_model.py into $MODELS_DIR"
