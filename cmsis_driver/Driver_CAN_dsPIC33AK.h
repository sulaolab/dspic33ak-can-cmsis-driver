#ifndef DRIVER_CAN_DSPIC33AK_H
#define DRIVER_CAN_DSPIC33AK_H

/*
 * CMSIS-Driver CAN wrapper for the dsPIC33AK CAN FD HAL.
 *
 * Thin mapping of the ARM CMSIS-Driver CAN API onto the dsPIC33AK CAN FD HAL
 * (blocking node API + optional interrupt/event layer). No ARM_CAN_* /
 * ARM_DRIVER_* types appear in the HAL; they are confined to this wrapper.
 *
 * Instance mapping:
 *   Driver_CAN1 -> DSPIC33AK_CANFD_INST_1  (C1)
 *   Driver_CAN2 -> DSPIC33AK_CANFD_INST_2  (C2)
 *
 * Initial scope (v0.x): CAN FD, NORMAL / internal+external loopback / monitor
 * (listen-only) modes; one transmit object (index 0) and one accept-all receive
 * object (index 1). RECEIVE / RECEIVE_OVERRUN / UNIT_BUS_OFF events are
 * delivered. SEND_COMPLETE and identifier filtering are reported as unsupported
 * in the capabilities (see README); unsupported calls return
 * ARM_DRIVER_ERROR_UNSUPPORTED.
 *
 * The caller must perform board bring-up (module power, CAN clock, PPS including
 * the RX input, transceiver STBY) BEFORE PowerControl(ARM_POWER_FULL), exactly
 * as for the HAL's dspic33ak_canfd_init().
 */

#include "Driver_CAN.h"

#ifdef __cplusplus
extern "C" {
#endif

extern ARM_DRIVER_CAN Driver_CAN1;
extern ARM_DRIVER_CAN Driver_CAN2;

/* Millisecond tick provider for HAL blocking-call timeouts. Weak default returns
 * 0 (timeouts disabled); override with e.g. GetTicks() in the application. */
uint32_t Driver_CAN_dsPIC33AK_GetMs(void);

#ifdef __cplusplus
}
#endif

#endif /* DRIVER_CAN_DSPIC33AK_H */
