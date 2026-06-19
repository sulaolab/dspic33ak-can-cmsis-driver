# CMSIS-Driver CAN — official API gap summary

How each `ARM_DRIVER_CAN` (Driver_CAN.h, API v1.3) entry maps onto this wrapper +
the dsPIC33AK CAN FD HAL.

Legend: **OK** = implemented; **gap** = not implemented (returns
`ARM_DRIVER_ERROR_UNSUPPORTED` / advertised unavailable); **partial** = mapped
with a documented limitation.

## Lifecycle / power

| API | Status | Notes |
|---|---|---|
| `GetVersion` | OK | API v1.3, wrapper v0.x |
| `GetCapabilities` | OK | `num_objects=2`, `fd_mode=1`, monitor + internal/external loopback = 1, restricted = 0 |
| `Initialize(cb_unit, cb_object)` | OK | stores callbacks |
| `Uninitialize` | OK | `dspic33ak_canfd_deinit` |
| `PowerControl(FULL/OFF)` | OK | FULL = HAL `init`; the board must do PMD/clock/PPS first. `LOW` = gap |
| `GetClock` | OK | returns the configured FCAN |

## Configuration

| API | Status | Notes |
|---|---|---|
| `SetBitrate(NOMINAL/FD_DATA, bitrate, bit_segments)` | partial | applies via `dspic33ak_canfd_set_bitrate`; the HAL derives the segment distribution from a sample point, so a non-zero `bit_segments` is used only to derive that sample point |
| `SetMode(NORMAL/MONITOR/LOOPBACK_INTERNAL/LOOPBACK_EXTERNAL)` | OK | re-applies the HAL config in the new mode |
| `SetMode(INITIALIZATION)` | partial | no-op (bring-up happens in `PowerControl(FULL)`) |
| `SetMode(RESTRICTED)` | gap | HAL has no restricted mode |

## Objects / filters

| API | Status | Notes |
|---|---|---|
| `ObjectGetCapabilities(obj)` | OK | obj 0 = TX (depth 4), obj 1 = RX (depth 4) |
| `ObjectConfigure(obj, TX/RX/INACTIVE)` | partial | fixed model: obj 0 = TX, obj 1 = RX; RTR-auto objects = gap |
| `ObjectSetFilter(...)` | gap | the RX object is accept-all only; exact/range/mask filters not implemented |

## Transfer / status / events

| API | Status | Notes |
|---|---|---|
| `MessageSend(0, ...)` | OK | CAN FD, BRS, 11/29-bit IDs; returns bytes accepted |
| `MessageRead(1, ...)` | OK | pops from the wrapper RX ring (filled in the ISR); returns bytes read |
| `Control(ARM_CAN_SET_FD_MODE)` | OK | re-applies config |
| `Control(ARM_CAN_ABORT_MESSAGE_SEND)` | OK | `dspic33ak_canfd_tx_abort` |
| `Control(RETRANSMISSION / TRANSCEIVER_DELAY)` | gap | not exposed (TDC is automatic) |
| `GetStatus` | OK | unit state (active / passive / bus-off) + TX/RX error counts from CxTREC |
| `SignalObjectEvent(RECEIVE)` | OK | RX FIFO drained to a ring in the ISR, then signalled |
| `SignalObjectEvent(RECEIVE_OVERRUN)` | OK | ring/FIFO overflow |
| `SignalObjectEvent(SEND_COMPLETE)` | gap | the HAL TX-complete interrupt path is not yet validated on this silicon |
| `SignalUnitEvent(BUS_OFF)` | OK | derived from CxTREC in the handler |
| `SignalUnitEvent(ACTIVE/WARNING/PASSIVE)` | partial | surfaced via `GetStatus` polling, not delivered as unit events |

## Summary

The wrapper covers the lifecycle, bitrate, mode, send/receive, status and the
receive/bus-off events needed for a working CMSIS-Driver CAN. The remaining gaps
are deliberate for the initial scope and are advertised through the capabilities
or returned as `ARM_DRIVER_ERROR_UNSUPPORTED`:

- **SEND_COMPLETE event** — pending validation of the HAL TX-complete interrupt
  on this device.
- **Identifier filtering** (`ObjectSetFilter`) — RX is accept-all only.
- **Restricted mode**, **retransmission / transceiver-delay control**, **LOW
  power**.
