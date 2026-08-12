# Upstream HAL Source

The files in this directory are a vendor copy of the NORA CAN FD HAL.

Upstream repository:

- Repository: https://github.com/sulaolab/nora-hal-dspic33ak-can
- Branch: main
- Source directory: src/

Synchronized into this repository under:

- Destination directory: src/hal_can/

The upstream repository was previously named `dspic33ak-hal-can` and its public API
was `dspic33ak_canfd_*`. Both changed together: the API is now `nora_canfd_*`, and
the `_dspic33ak` tag survives only on backend implementation files
(`nora_canfd_*_dspic33ak.c`, `nora_canfd_dspic33ak_reg.h`). There are no
compatibility aliases — the old names are gone, not deprecated.

## Current Synchronized Revision

- Upstream commit: 7767f495053f9633df7f7e67ba6fcc7854d48693

This revision is on upstream `main`: the fleet-wide NORA landing (2026-08-13)
fast-forwarded `main` onto the former `refactor/nora-hal` branch, so plain
`python tools/sync_hal_from_upstream.py` pulls exactly these bytes.

This revision also carries two entry points the previous vendored copy predated:
`nora_canfd_clear_rx_overflow()` and `nora_canfd_module_enable()`. Both are
additions; nothing was removed.

## Update Policy

The HAL-only repository is the upstream source of truth for this directory: apply
HAL fixes and HAL feature changes there, then synchronize this vendor copy. Do not
edit `src/hal_can/` in place — every file here is byte-identical to upstream, and
`tools/sync_hal_from_upstream.py` overwrites local edits without warning.

Note that the HAL-only repository is itself a published snapshot rather than the
place the code is written. HAL changes are made and hardware-validated in the
integration projects (the audio-DSP validation project and the HAL starter), then
published to the standalone HAL repository, and only then vendored here. A HAL fix
therefore has two hops to travel before it reaches this repository.

CMSIS-Driver wrapper changes belong in this repository.
