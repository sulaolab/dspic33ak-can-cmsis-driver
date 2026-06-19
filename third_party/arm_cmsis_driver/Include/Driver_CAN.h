/*
 * Copyright (c) 2015-2020 ARM Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * $Date:        31. March 2020
 * $Revision:    V1.3
 *
 * Project:      CAN (Controller Area Network) Driver definitions
 */

/* History:
 *  Version 1.3
 *    Removed volatile from ARM_CAN_STATUS
 *  Version 1.2
 *    Removed function ARM_CAN_ObjectGetMessageCount and added ARM_CAN_STATUS
 *  Version 1.1
 *    ARM_CAN_STATUS made volatile
 *  Version 1.0
 *    Initial release
 */

#ifndef DRIVER_CAN_H_
#define DRIVER_CAN_H_

#ifdef  __cplusplus
extern "C"
{
#endif

#include "Driver_Common.h"

#define ARM_CAN_API_VERSION ARM_DRIVER_VERSION_MAJOR_MINOR(1,3)  /* API version */

#define _ARM_Driver_CAN_(n)      Driver_CAN##n
#define  ARM_Driver_CAN_(n) _ARM_Driver_CAN_(n)


/****** CAN Bitrate selection codes *****/
typedef enum _ARM_CAN_BITRATE_SELECT {
  ARM_CAN_BITRATE_NOMINAL,              ///< Select nominal (arbitration) bitrate
  ARM_CAN_BITRATE_FD_DATA               ///< Select FD data bitrate
} ARM_CAN_BITRATE_SELECT;

/****** CAN Bitrate setting codes (bit segments) *****/
#define ARM_CAN_BIT_PROP_SEG_Pos         0UL                                                ///< bit position of PROP_SEG field
#define ARM_CAN_BIT_PROP_SEG_Msk        (0xFFUL << ARM_CAN_BIT_PROP_SEG_Pos)                ///< bit mask     of PROP_SEG field
#define ARM_CAN_BIT_PROP_SEG(x)        (((x)    << ARM_CAN_BIT_PROP_SEG_Pos) & ARM_CAN_BIT_PROP_SEG_Msk)   ///< PROP_SEG setting
#define ARM_CAN_BIT_PHASE_SEG1_Pos       8UL                                                ///< bit position of PHASE_SEG1 field
#define ARM_CAN_BIT_PHASE_SEG1_Msk      (0xFFUL << ARM_CAN_BIT_PHASE_SEG1_Pos)              ///< bit mask     of PHASE_SEG1 field
#define ARM_CAN_BIT_PHASE_SEG1(x)      (((x)    << ARM_CAN_BIT_PHASE_SEG1_Pos) & ARM_CAN_BIT_PHASE_SEG1_Msk) ///< PHASE_SEG1 setting
#define ARM_CAN_BIT_PHASE_SEG2_Pos      16UL                                                ///< bit position of PHASE_SEG2 field
#define ARM_CAN_BIT_PHASE_SEG2_Msk      (0xFFUL << ARM_CAN_BIT_PHASE_SEG2_Pos)              ///< bit mask     of PHASE_SEG2 field
#define ARM_CAN_BIT_PHASE_SEG2(x)      (((x)    << ARM_CAN_BIT_PHASE_SEG2_Pos) & ARM_CAN_BIT_PHASE_SEG2_Msk) ///< PHASE_SEG2 setting
#define ARM_CAN_BIT_SJW_Pos             24UL                                                ///< bit position of SJW field
#define ARM_CAN_BIT_SJW_Msk             (0xFFUL << ARM_CAN_BIT_SJW_Pos)                     ///< bit mask     of SJW field
#define ARM_CAN_BIT_SJW(x)             (((x)    << ARM_CAN_BIT_SJW_Pos) & ARM_CAN_BIT_SJW_Msk) ///< SJW setting

/****** CAN Mode codes *****/
typedef enum _ARM_CAN_MODE {
  ARM_CAN_MODE_INITIALIZATION,          ///< Initialization mode
  ARM_CAN_MODE_NORMAL,                  ///< Normal operation mode
  ARM_CAN_MODE_RESTRICTED,              ///< Restricted operation mode
  ARM_CAN_MODE_MONITOR,                 ///< Bus monitoring mode
  ARM_CAN_MODE_LOOPBACK_INTERNAL,       ///< Loopback internal mode
  ARM_CAN_MODE_LOOPBACK_EXTERNAL        ///< Loopback external mode
} ARM_CAN_MODE;

/****** CAN Filter Operation codes *****/
typedef enum _ARM_CAN_FILTER_OPERATION {
  ARM_CAN_FILTER_ID_EXACT_ADD,          ///< Add    exact id filter
  ARM_CAN_FILTER_ID_EXACT_REMOVE,       ///< Remove exact id filter
  ARM_CAN_FILTER_ID_RANGE_ADD,          ///< Add    range id filter
  ARM_CAN_FILTER_ID_RANGE_REMOVE,       ///< Remove range id filter
  ARM_CAN_FILTER_ID_MASKABLE_ADD,       ///< Add    maskable id filter
  ARM_CAN_FILTER_ID_MASKABLE_REMOVE     ///< Remove maskable id filter
} ARM_CAN_FILTER_OPERATION;

/****** CAN Object Configuration codes *****/
typedef enum _ARM_CAN_OBJ_CONFIG {
  ARM_CAN_OBJ_INACTIVE,                 ///< CAN object inactive
  ARM_CAN_OBJ_TX,                       ///< CAN transmit object
  ARM_CAN_OBJ_RX,                       ///< CAN receive object
  ARM_CAN_OBJ_RX_RTR_TX_DATA,           ///< CAN object that on RTR reception automatically transmits Data Frame
  ARM_CAN_OBJ_TX_RTR_RX_DATA            ///< CAN object that transmits RTR and receives Data Frame
} ARM_CAN_OBJ_CONFIG;

/**
\brief CAN Object Capabilities
*/
typedef struct _ARM_CAN_OBJ_CAPABILITIES {
  uint32_t tx               :  1;       ///< Object supports transmission
  uint32_t rx               :  1;       ///< Object supports reception
  uint32_t rx_rtr_tx_data   :  1;       ///< Object supports RTR reception and automatic Data Frame transmission
  uint32_t tx_rtr_rx_data   :  1;       ///< Object supports RTR transmission and reception of Data Frame
  uint32_t multiple_filters :  1;       ///< Object allows assignment of multiple filters to it
  uint32_t exact_filtering  :  1;       ///< Object supports exact identifier filtering
  uint32_t range_filtering  :  1;       ///< Object supports range identifier filtering
  uint32_t mask_filtering   :  1;       ///< Object supports mask identifier filtering
  uint32_t message_depth    : 16;       ///< Number of messages buffer-able by the object
  uint32_t reserved         :  8;       ///< Reserved (must be zero)
} ARM_CAN_OBJ_CAPABILITIES;

/****** CAN Unit State codes *****/
#define ARM_CAN_UNIT_STATE_INACTIVE     (0U)   ///< Unit not initialized / not active
#define ARM_CAN_UNIT_STATE_ACTIVE       (1U)   ///< Unit active (error active)
#define ARM_CAN_UNIT_STATE_PASSIVE      (2U)   ///< Unit error passive
#define ARM_CAN_UNIT_STATE_BUS_OFF      (3U)   ///< Unit bus-off

/****** CAN Last Error Code (LEC) values *****/
#define ARM_CAN_LEC_NO_ERROR            (0U)   ///< No error
#define ARM_CAN_LEC_BIT_ERROR           (1U)   ///< Bit error
#define ARM_CAN_LEC_STUFF_ERROR         (2U)   ///< Bit stuffing error
#define ARM_CAN_LEC_CRC_ERROR           (3U)   ///< CRC error
#define ARM_CAN_LEC_FORM_ERROR          (4U)   ///< Illegal fixed-form bit
#define ARM_CAN_LEC_ACK_ERROR           (5U)   ///< Acknowledgement error

/**
\brief CAN Status
*/
typedef struct _ARM_CAN_STATUS {
  uint32_t unit_state      :  4;        ///< Unit bus state (ARM_CAN_UNIT_STATE_xxx)
  uint32_t last_error_code :  4;        ///< Last error code (ARM_CAN_LEC_xxx)
  uint32_t tx_error_count  :  8;        ///< Transmitter error count
  uint32_t rx_error_count  :  8;        ///< Receiver error count
  uint32_t reserved        :  8;
} ARM_CAN_STATUS;

/****** CAN Identifier encoding macros *****/
#define ARM_CAN_STANDARD_ID(id)         (id & 0x000007FFUL)                       ///< Standard (11-bit) identifier
#define ARM_CAN_EXTENDED_ID(id)        ((id & 0x1FFFFFFFUL) | 0x80000000UL)       ///< Extended (29-bit) identifier
#define ARM_CAN_ID_IDE_Msk              0x80000000UL                              ///< Identifier extension bit mask

/**
\brief CAN Message Information
*/
typedef struct _ARM_CAN_MSG_INFO {
  uint32_t id;                          ///< CAN identifier with frame format specifier (bit 31 = IDE)
  uint32_t rtr      :  1;               ///< Remote Transmission Request frame
  uint32_t edl      :  1;               ///< Extended Data Length (CAN FD)
  uint32_t brs      :  1;               ///< Bit Rate Switch (CAN FD)
  uint32_t esi      :  1;               ///< Error State Indicator (CAN FD)
  uint32_t dlc      :  4;               ///< Data Length Code
  uint32_t reserved : 24;
} ARM_CAN_MSG_INFO;

/****** CAN Control Function Operation codes *****/
#define ARM_CAN_CONTROL_Pos              0UL
#define ARM_CAN_CONTROL_Msk             (0xFFUL << ARM_CAN_CONTROL_Pos)
#define ARM_CAN_SET_FD_MODE             (1UL << ARM_CAN_CONTROL_Pos)   ///< Set FD operation; arg: 0 = disable, 1 = enable
#define ARM_CAN_ABORT_MESSAGE_SEND      (2UL << ARM_CAN_CONTROL_Pos)   ///< Abort sending of CAN message; arg = object
#define ARM_CAN_CONTROL_RETRANSMISSION  (3UL << ARM_CAN_CONTROL_Pos)   ///< Enable/disable automatic retransmission; arg: 0/1
#define ARM_CAN_SET_TRANSCEIVER_DELAY   (4UL << ARM_CAN_CONTROL_Pos)   ///< Set transceiver delay; arg = delay

/****** CAN Control Argument codes *****/
#define ARM_CAN_FD_MODE_DISABLE         (0UL)  ///< Disable CAN FD operation
#define ARM_CAN_FD_MODE_ENABLE          (1UL)  ///< Enable  CAN FD operation
#define ARM_CAN_RETRANSMISSION_DISABLE  (0UL)  ///< Disable automatic retransmission
#define ARM_CAN_RETRANSMISSION_ENABLE   (1UL)  ///< Enable  automatic retransmission

/****** CAN Unit Event *****/
#define ARM_CAN_EVENT_UNIT_INACTIVE     (0U)   ///< Unit entered Inactive state
#define ARM_CAN_EVENT_UNIT_ACTIVE       (1U)   ///< Unit entered Error Active state
#define ARM_CAN_EVENT_UNIT_WARNING      (2U)   ///< Unit entered Error Warning state (one of the error counters >= 96)
#define ARM_CAN_EVENT_UNIT_PASSIVE      (3U)   ///< Unit entered Error Passive state
#define ARM_CAN_EVENT_UNIT_BUS_OFF      (4U)   ///< Unit entered Bus-off state

/****** CAN Object Event *****/
#define ARM_CAN_EVENT_SEND_COMPLETE     (1U)   ///< Message was sent successfully on the object
#define ARM_CAN_EVENT_RECEIVE           (2U)   ///< Message was received successfully on the object
#define ARM_CAN_EVENT_RECEIVE_OVERRUN   (3U)   ///< Message was overrun on the object

// Function documentation is provided in the CMSIS-Driver documentation.
typedef void (*ARM_CAN_SignalUnitEvent_t)   (uint32_t event);                   ///< Pointer to ARM_CAN_SignalUnitEvent   : Signal CAN Unit Event.
typedef void (*ARM_CAN_SignalObjectEvent_t) (uint32_t obj_idx, uint32_t event); ///< Pointer to ARM_CAN_SignalObjectEvent : Signal CAN Object Event.

/**
\brief CAN Device Driver Capabilities.
*/
typedef struct _ARM_CAN_CAPABILITIES {
  uint32_t num_objects         :  8;    ///< Number of message objects supported
  uint32_t reentrant_operation :  1;    ///< Support for reentrant calls to MessageSend / MessageRead / ObjectConfigure
  uint32_t fd_mode             :  1;    ///< Support for CAN FD mode
  uint32_t restricted_mode     :  1;    ///< Support for restricted operation mode
  uint32_t monitor_mode        :  1;    ///< Support for bus monitoring mode
  uint32_t internal_loopback   :  1;    ///< Support for internal loopback mode
  uint32_t external_loopback   :  1;    ///< Support for external loopback mode
  uint32_t reserved            : 18;    ///< Reserved (must be zero)
} ARM_CAN_CAPABILITIES;

/**
\brief Access structure of the CAN Driver.
*/
typedef struct _ARM_DRIVER_CAN {
  ARM_DRIVER_VERSION       (*GetVersion)           (void);                                                                            ///< Get driver version.
  ARM_CAN_CAPABILITIES     (*GetCapabilities)      (void);                                                                            ///< Get driver capabilities.
  int32_t                  (*Initialize)           (ARM_CAN_SignalUnitEvent_t   cb_unit_event,
                                                    ARM_CAN_SignalObjectEvent_t cb_object_event);                                     ///< Initialize CAN interface.
  int32_t                  (*Uninitialize)         (void);                                                                            ///< De-initialize CAN interface.
  int32_t                  (*PowerControl)         (ARM_POWER_STATE state);                                                           ///< Control CAN interface power.
  uint32_t                 (*GetClock)             (void);                                                                            ///< Retrieve CAN base clock frequency.
  int32_t                  (*SetBitrate)           (ARM_CAN_BITRATE_SELECT select, uint32_t bitrate, uint32_t bit_segments);          ///< Set bitrate for CAN interface.
  int32_t                  (*SetMode)              (ARM_CAN_MODE mode);                                                               ///< Set operating mode.
  ARM_CAN_OBJ_CAPABILITIES (*ObjectGetCapabilities)(uint32_t obj_idx);                                                                ///< Retrieve capabilities of an object.
  int32_t                  (*ObjectSetFilter)      (uint32_t obj_idx, ARM_CAN_FILTER_OPERATION operation, uint32_t id, uint32_t arg); ///< Add or remove filter for message reception.
  int32_t                  (*ObjectConfigure)      (uint32_t obj_idx, ARM_CAN_OBJ_CONFIG obj_cfg);                                    ///< Configure object.
  int32_t                  (*MessageSend)          (uint32_t obj_idx, ARM_CAN_MSG_INFO *msg_info, const uint8_t *data, uint8_t size); ///< Send message on CAN bus.
  int32_t                  (*MessageRead)          (uint32_t obj_idx, ARM_CAN_MSG_INFO *msg_info,       uint8_t *data, uint8_t size); ///< Read message received on CAN bus.
  int32_t                  (*Control)              (uint32_t control, uint32_t arg);                                                  ///< Control CAN interface.
  ARM_CAN_STATUS           (*GetStatus)            (void);                                                                            ///< Get CAN status.
} const ARM_DRIVER_CAN;

#ifdef  __cplusplus
}
#endif

#endif /* DRIVER_CAN_H_ */
