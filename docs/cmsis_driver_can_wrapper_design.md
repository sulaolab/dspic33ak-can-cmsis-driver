# CMSIS-Driver CAN wrapper — design notes

## Goal

Present the dsPIC33AK CAN FD HAL through the ARM CMSIS-Driver CAN API
(`ARM_DRIVER_CAN`, Driver_CAN.h v1.3) without leaking any `ARM_CAN_*` /
`ARM_DRIVER_*` types into the HAL. The wrapper is a thin adapter; all CAN
protocol work stays in the HAL.

## Structure

`Driver_CAN_dsPIC33AK.c` follows the same shape as the sibling I2C/USART
wrappers:

* An index-based core (`CAN_GetVersion`, `CAN_Initialize(index, ...)`, ...).
* A per-instance access-struct generator macro that emits `Driver_CAN1` /
  `Driver_CAN2`, mapping them to HAL instances C1 / C2.
* Static driver/capabilities version structures.
* A weak `Driver_CAN_dsPIC33AK_GetMs()` the application overrides for the HAL's
  blocking-call timeouts.

The wrapper depends only on the HAL public headers (`dspic33ak_canfd_node.h`,
`dspic33ak_canfd_isr.h`) and the vendored ARM headers.

## Object model

Fixed two objects: object 0 = transmit (HAL TX queue), object 1 = receive (HAL
RX FIFO 1, accept-all). `ObjectGetCapabilities` reports these; `ObjectConfigure`
accepts the matching TX/RX/inactive configuration and rejects the rest.

## Receive path (why a ring)

The HAL RX interrupt is **signal-only**: the HAL `irq_handler` raises an
`RX_AVAILABLE` event but does not drain the hardware RX FIFO — the consumer must.
CMSIS-Driver CAN delivers `ARM_CAN_EVENT_RECEIVE` and the application reads later
with `MessageRead`. If the FIFO were left undrained until `MessageRead`, the
level-sensitive RX interrupt would re-assert continuously and starve the main
loop.

So the wrapper's HAL event callback (ISR context) **drains the hardware RX FIFO
into a small per-instance software ring** and then signals `RECEIVE`;
`MessageRead` pops from that ring. This matches the CMSIS model (the driver
buffers received frames; `MessageRead` retrieves them) and keeps the ISR short.

## Transmit path

`MessageSend` builds a HAL frame from `ARM_CAN_MSG_INFO` (IDE bit -> extended,
`edl` -> FD, `brs`, length) and calls the HAL blocking `transmit` (queue). It
returns the number of bytes accepted. `SEND_COMPLETE` is not signalled (the HAL
TX-complete interrupt path is unvalidated on this device), so the capability is
advertised accordingly.

## Mode / bitrate

`SetMode` maps the ARM mode to a HAL mode and re-applies the configuration in
that mode. `SetBitrate` caches the nominal/data rate and calls the HAL
`set_bitrate` (config-mode drop + restore); a non-zero ARM `bit_segments` is used
only to derive a sample point, since the HAL computes the concrete segment
distribution.

## Status / events

`GetStatus` decodes the HAL bus-status (CxTREC) into the CMSIS unit state
(active / passive / bus-off) and the TX/RX error counters. `SignalUnitEvent` for
bus-off is delivered from the handler; warning/passive transitions are surfaced
through `GetStatus` polling rather than as unit events in this initial scope.

## Vectors

The HAL does not own interrupt vectors; on the dsPIC33AK the CAN module raises
separate RX / TX / general CPU vectors, and the application forwards all of them
to `dspic33ak_canfd_irq_handler()`. The wrapper registers its event callback with
the HAL ISR layer in `PowerControl(ARM_POWER_FULL)`.

## Board bring-up

The wrapper does not touch power/clock/pins (the same policy as the HAL and the
sibling wrappers). The application enables the module, starts the CAN clock and
maps PPS before `PowerControl(ARM_POWER_FULL)`.
