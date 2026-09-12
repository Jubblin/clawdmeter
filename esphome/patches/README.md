# patches/

`clawdmeter_amoled_206_model.py` registers a `WAVESHARE-ESP32-S3-TOUCH-AMOLED-2.06`
model with ESPHome's `mipi_spi` component (the CO5300 QSPI AMOLED driver),
alongside the `WAVESHARE-ESP32-S3-TOUCH-AMOLED-2.16` and `-1.75` models that
ship in `esphome/components/mipi_spi/models/waveshare.py` upstream (added
for the 2.16 in ESPHome 2026.6.0, PR esphome/esphome#16887).

`mipi_spi`'s model registry (`esphome/components/mipi_spi/models/__init__.py`)
auto-discovers every Python module dropped into that directory via
`pkgutil.iter_modules()` — it isn't a fixed list. `apply.sh` copies our one
small file into an installed esphome package's copy of that directory so
`model: WAVESHARE-ESP32-S3-TOUCH-AMOLED-2.06` resolves in `amoled_206.yaml`.

## Why a copy-in patch instead of `external_components:`

ESPHome lets you override a core component entirely via
`external_components:` pointing at a local directory with the same
component name — but that requires shipping the **whole** component
(`mipi_spi/` is ~3100 lines across `display.py`, `mipi_spi.cpp/.h`, and
`models/*.py` as of ESPHome 2026.6.x). Vendoring all of that to add one
12-line model registration would immediately drift from upstream every time
ESPHome ships a `mipi_spi` change. Dropping one file into a directory
that's designed to be flat and auto-scanned avoids that entirely, at the
cost of needing to re-run `apply.sh` whenever the esphome install/venv is
recreated.

## Usage

```bash
esphome/patches/apply.sh                       # uses `python3` on PATH
esphome/patches/apply.sh /path/to/venv/bin/python3   # or a specific venv
```

Run this before every `esphome config` / `compile` / `run` against
`amoled_206.yaml` until the model is upstreamed.

## The real fix

Upstream this model to `esphome/esphome`
(`esphome/components/mipi_spi/models/waveshare.py`) the same way the 2.16
and 1.75 were added — see PR esphome/esphome#16887 for the precedent. Once
merged (and released), `patches/` and the `apply.sh` step go away entirely.
