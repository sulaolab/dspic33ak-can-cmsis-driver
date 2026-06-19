# CMSIS-Driver CAN Wrapper for dsPIC33AK CAN FD HAL

## Overview

`Driver_CAN_dsPIC33AK.{c,h}` maps the ARM CMSIS-Driver **CAN** API
(`Driver_CAN.h`, API v1.3) onto the dsPIC33AK CAN FD HAL: the blocking node API
(`init` / `set_bitrate` / `transmit` / `receive`) plus the optional
interrupt/event layer (`isr_enable` + event callback). All `ARM_CAN_*` /
`ARM_DRIVER_*` types are confined to the wrapper; none appear in the HAL.

## Instance mapping

| CMSIS driver object | dsPIC33AK HAL instance |
|---|---|
| `Driver_CAN1` | `DSPIC33AK_CANFD_INST_1` (C1) |
| `Driver_CAN2` | `DSPIC33AK_CANFD_INST_2` (C2) |

## Object model

Fixed two-object model: object 0 = transmit (TX queue), object 1 = receive
(RX FIFO 1, accept-all filter). The wrapper drains the RX FIFO in the ISR into a
small per-instance ring and signals `ARM_CAN_EVENT_RECEIVE`; `MessageRead` pops
from that ring (the HAL RX interrupt is signal-only, so this avoids an RX
interrupt storm when the read is deferred).

## Supported features

- `Initialize` / `Uninitialize` / `PowerControl(FULL/OFF)`
- `GetClock`
- `SetBitrate(NOMINAL / FD_DATA, ...)` via `dspic33ak_canfd_set_bitrate()`; a
  non-zero ARM `bit_segments` is used only to derive the sample point
- `SetMode`: `NORMAL`, `MONITOR` (listen-only), `LOOPBACK_INTERNAL`,
  `LOOPBACK_EXTERNAL`; `INITIALIZATION` is a no-op (bring-up in `PowerControl`)
- `MessageSend(0, ...)` / `MessageRead(1, ...)` — CAN FD, BRS, 11/29-bit IDs
- `Control(ARM_CAN_SET_FD_MODE)` and `Control(ARM_CAN_ABORT_MESSAGE_SEND)`
- `GetStatus` — unit state (active / passive / bus-off) + TX/RX error counts
- Events: `ARM_CAN_EVENT_RECEIVE` / `RECEIVE_OVERRUN` (object 1) and
  `ARM_CAN_EVENT_UNIT_BUS_OFF`

## Unsupported features (initial scope)

Unsupported calls return `ARM_DRIVER_ERROR_UNSUPPORTED`; the capabilities
advertise what is available.

- `ARM_CAN_EVENT_SEND_COMPLETE` is not signalled (the HAL TX-complete interrupt
  path is not yet validated on this silicon).
- `ObjectSetFilter` (exact/range/mask identifier filtering): the RX object is
  accept-all only.
- `ARM_CAN_MODE_RESTRICTED`, `Control(ARM_CAN_CONTROL_RETRANSMISSION)`,
  `Control(ARM_CAN_SET_TRANSCEIVER_DELAY)`, `ARM_POWER_LOW`.

## Board bring-up (caller responsibility)

As with the HAL `dspic33ak_canfd_init()`, the wrapper does NOT touch
power/clock/pins. Before `PowerControl(ARM_POWER_FULL)` the application must
enable the module (`PMD3.CxMD = 0`), start the CAN clock (FCAN), map PPS (TX out,
**RX in — required even for loopback**) and drive the transceiver standby.

## Configuration (RTE_Device_CAN_dsPIC33AK_example.h)

`RTE_CAN1` / `RTE_CAN2` enable the drivers; `RTE_CANx_CLK_HZ`,
`RTE_CANx_NOMINAL_BPS`, `RTE_CANx_DATA_BPS`, `RTE_CANx_SAMPLE_PCT`,
`RTE_CANx_FD_MODE`, `RTE_CANx_TIMEOUT_MS`, `RTE_CANx_IRQ_PRIORITY` provide the
defaults applied at `PowerControl(ARM_POWER_FULL)`.

## Tick provider

A weak `Driver_CAN_dsPIC33AK_GetMs()` returns 0 (timeouts disabled); override it
(e.g. `return systick_ms();`) for blocking-call timeouts.

## Interrupt vectors

The dsPIC33AK CAN module raises separate RX / TX / general CPU vectors; the
application forwards all of them to `dspic33ak_canfd_irq_handler()`:

```c
void __attribute__((interrupt, no_auto_psv)) _C1RXInterrupt(void) { dspic33ak_canfd_irq_handler(DSPIC33AK_CANFD_INST_1); }
void __attribute__((interrupt, no_auto_psv)) _C1TXInterrupt(void) { dspic33ak_canfd_irq_handler(DSPIC33AK_CANFD_INST_1); }
void __attribute__((interrupt, no_auto_psv)) _C1Interrupt  (void) { dspic33ak_canfd_irq_handler(DSPIC33AK_CANFD_INST_1); }
```

## Basic usage

```c
#include "Driver_CAN_dsPIC33AK.h"
extern ARM_DRIVER_CAN Driver_CAN1;

/* ... board bring-up: PMD, CAN clock, PPS (TX/RX), STBY ... */

Driver_CAN1.Initialize(unit_event_cb, object_event_cb);
Driver_CAN1.PowerControl(ARM_POWER_FULL);          /* comes up in NORMAL (FD) */
Driver_CAN1.SetBitrate(ARM_CAN_BITRATE_NOMINAL, 500000u, 0u);
Driver_CAN1.SetBitrate(ARM_CAN_BITRATE_FD_DATA, 2000000u, 0u);

ARM_CAN_MSG_INFO tx = { .id = ARM_CAN_STANDARD_ID(0x0A0), .edl = 1, .brs = 1 };
uint8_t data[8] = { 0,1,2,3,4,5,6,7 };
Driver_CAN1.MessageSend(0u, &tx, data, 8u);

/* in object_event_cb on ARM_CAN_EVENT_RECEIVE (obj 1): */
ARM_CAN_MSG_INFO rx;
uint8_t rxd[64];
Driver_CAN1.MessageRead(1u, &rx, rxd, sizeof(rxd));
```
