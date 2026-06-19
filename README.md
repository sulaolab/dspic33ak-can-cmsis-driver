# dspic33ak-can-cmsis-driver

CMSIS-Driver CAN wrapper package for the dsPIC33AK CAN FD HAL.

This repository provides a CMSIS-Driver CAN wrapper (ARM `Driver_CAN.h`, API
v1.3) together with a vendor copy of the dsPIC33AK CAN FD HAL.

## Repository Layout

```text
src/
  hal_can/
    dspic33ak_canfd.h          (instance / status / mode types, lifecycle)
    dspic33ak_canfd_reg.h
    dspic33ak_canfd_device.h   dspic33ak_canfd_device.c
    dspic33ak_canfd_common.h   dspic33ak_canfd_common.c
    dspic33ak_canfd_node.h     dspic33ak_canfd_node.c
    dspic33ak_canfd_isr.h      dspic33ak_canfd_isr.c
    UPSTREAM.md

tools/
  sync_hal_from_upstream.py

cmsis_driver/
  Driver_CAN_dsPIC33AK.c
  Driver_CAN_dsPIC33AK.h
  RTE_Device_CAN_dsPIC33AK_example.h
  README.md

third_party/
  arm_cmsis_driver/
    README.md
    LICENSE.txt
    Include/
      Driver_Common.h
      Driver_CAN.h

docs/
  cmsis_driver_can_wrapper_design.md
  cmsis_can_official_gap_summary.md
```

## Current Status

The HAL vendor copy has been imported from:

- https://github.com/sulaolab/dspic33ak-can-hal

The CMSIS-Driver wrapper files are provided under `cmsis_driver/`. The wrapper
maps the ARM CMSIS-Driver CAN API onto the CAN FD HAL: `Driver_CAN1` / `Driver_CAN2`
map to HAL instances C1 / C2; one transmit object (0) and one accept-all receive
object (1); RECEIVE / RECEIVE_OVERRUN / UNIT_BUS_OFF events are delivered. It has
been validated end to end on a dsPIC33AK512MPS512 (Initialize -> PowerControl ->
SetBitrate -> SetMode/MessageSend -> RECEIVE event -> MessageRead). See
`cmsis_driver/README.md` for the supported/unsupported feature list and
`docs/cmsis_can_official_gap_summary.md` for the full API mapping.

`RTE_Device_CAN_dsPIC33AK_example.h` is a CAN-only example configuration file.
The driver includes `RTE_Device.h` when one is on the include path and otherwise
falls back to this bundled example, so it builds standalone. Integrated
applications should copy the required `RTE_CANx` definitions into their own
application-level `RTE_Device.h` or equivalent configuration header.

A minimal copy of the ARM CMSIS-Driver API headers (`Driver_CAN.h`,
`Driver_Common.h`) is vendored under `third_party/arm_cmsis_driver/Include/` so
the wrapper builds without a separate CMSIS installation. See
`third_party/arm_cmsis_driver/README.md` for the source and license.

## Include Path

Applications or build systems should provide include paths for:

```text
src/hal_can
cmsis_driver
third_party/arm_cmsis_driver/Include
```

`Driver_CAN.h` is resolved from the vendored ARM CMSIS-Driver headers under
`third_party/arm_cmsis_driver/Include/` (Apache-2.0, copied unmodified). A
different CMSIS-Driver package may be substituted by adjusting this include path.

## HAL Synchronization

The HAL vendor copy under `src/hal_can/` can be synchronized from the upstream
HAL-only repository using:

```powershell
# Windows (Python launcher)
py -3 tools/sync_hal_from_upstream.py

# or, if python is on PATH
python tools/sync_hal_from_upstream.py
```

## Upstream HAL Policy

The HAL-only repository is the upstream source of truth:

- https://github.com/sulaolab/dspic33ak-can-hal

HAL fixes should be applied to the upstream HAL repository first, then
synchronized into this repository. CMSIS-Driver wrapper changes should be made in
this repository.

## License

The original dsPIC33AK CAN CMSIS-Driver wrapper code in this repository is
licensed under the MIT No Attribution License (MIT-0). See `LICENSE`.

The vendored ARM CMSIS-Driver headers under `third_party/arm_cmsis_driver/` are
provided by ARM under the Apache License 2.0. See
`third_party/arm_cmsis_driver/LICENSE.txt`.
