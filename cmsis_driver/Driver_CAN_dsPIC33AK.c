/* ========================================================================== */
/* CMSIS-Driver CAN wrapper for the dsPIC33AK CAN FD HAL (C1 / C2)             */
/* ========================================================================== */

/*
 * Maps the ARM CMSIS-Driver CAN API onto the dsPIC33AK CAN FD HAL: the blocking
 * node API (init / set_bitrate / transmit / receive) plus the optional
 * interrupt/event layer (isr_enable + event callback) for RX events. All
 * ARM_CAN_* / ARM_DRIVER_* types live here, never in the HAL.
 *
 * Object model (fixed, initial scope):
 *   obj 0 = transmit object (TX queue)
 *   obj 1 = receive object  (RX FIFO 1, accept-all filter)
 *
 * Delivered events: ARM_CAN_EVENT_RECEIVE / RECEIVE_OVERRUN (obj 1) and
 * ARM_CAN_EVENT_UNIT_BUS_OFF. SEND_COMPLETE is NOT signalled (the HAL TX-complete
 * interrupt path is not yet validated on this silicon), so capabilities and the
 * TX object advertise it as unavailable - mirroring how the USART wrapper sets
 * event_tx_complete = 0.
 *
 * Identifier filtering: only accept-all is supported initially, so
 * ObjectSetFilter returns ARM_DRIVER_ERROR_UNSUPPORTED.
 *
 * The signal callbacks may run in CAN ISR context: keep them short, do not block,
 * and do not call blocking driver APIs from them.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "Driver_CAN_dsPIC33AK.h"
#include "RTE_Device_CAN_dsPIC33AK_example.h"
#include "dspic33ak_canfd_node.h"
#include "dspic33ak_canfd_isr.h"

#ifndef ARM_DRIVER_VERSION_MAJOR_MINOR
#define ARM_DRIVER_VERSION_MAJOR_MINOR(major, minor) (((major) << 8) | (minor))
#endif

#ifndef ARM_CAN_API_VERSION
#define ARM_CAN_API_VERSION ARM_DRIVER_VERSION_MAJOR_MINOR(1, 3)
#endif

/* This wrapper's own implementation version (0.x = initial / pre-release). */
#define DRIVER_CAN_DSPIC33AK_VERSION ARM_DRIVER_VERSION_MAJOR_MINOR(0, 1)

/* Fixed object indices (see file header). */
#define CAN_OBJ_TX  0u
#define CAN_OBJ_RX  1u
#define CAN_OBJ_COUNT 2u

/* Caller-owned CAN message RAM (one region per instance, 4-byte aligned). Sized
 * to the HAL's fixed geometry (TXQ + RX FIFO 1); the HAL rejects an undersized
 * region at init. */
#define CAN_MSG_RAM_BYTES 576u
#define CAN_MSG_RAM_WORDS ((CAN_MSG_RAM_BYTES + 3u) / 4u)

/* Software receive ring: the ISR drains the hardware RX FIFO into here so the
 * RX interrupt does not re-storm before the application calls MessageRead (the
 * HAL RX event is signal-only; it does not drain the FIFO itself). */
#define CAN_RX_RING_LEN 8u

static uint32_t can1_msg_ram[CAN_MSG_RAM_WORDS] __attribute__((aligned(4)));
static uint32_t can2_msg_ram[CAN_MSG_RAM_WORDS] __attribute__((aligned(4)));

/* ========================================================================== */
/* Driver version and capabilities                                            */
/* ========================================================================== */

static const ARM_DRIVER_VERSION can_driver_version = {
    ARM_CAN_API_VERSION,
    DRIVER_CAN_DSPIC33AK_VERSION
};

/* CAN FD; NORMAL / monitor (listen-only) / internal + external loopback; no
 * restricted mode. Two fixed objects. */
static const ARM_CAN_CAPABILITIES can_capabilities = {
    CAN_OBJ_COUNT,  /* num_objects         */
    0u,             /* reentrant_operation */
    1u,             /* fd_mode             */
    0u,             /* restricted_mode     */
    1u,             /* monitor_mode        */
    1u,             /* internal_loopback   */
    1u,             /* external_loopback   */
    0u              /* reserved            */
};

/* ========================================================================== */
/* Per-instance context                                                       */
/* ========================================================================== */

typedef struct {
    dspic33ak_canfd_instance_t  hal_inst;
    uint32_t                   *msg_ram;
    uint16_t                    msg_ram_size;

    ARM_CAN_SignalUnitEvent_t   cb_unit;
    ARM_CAN_SignalObjectEvent_t cb_object;

    uint32_t can_clk_hz;
    uint32_t nominal_bps;
    uint32_t data_bps;
    uint8_t  sample_pct;
    uint8_t  fd_mode;
    uint8_t  irq_priority;
    uint32_t timeout_ms;

    uint8_t  enabled;       /* RTE_CANx */
    uint8_t  initialized;
    uint8_t  powered;

    dspic33ak_canfd_mode_t mode;   /* HAL operating mode currently applied */

    /* RX ring (producer = ISR drain, consumer = MessageRead). */
    dspic33ak_canfd_frame_t rx_ring[CAN_RX_RING_LEN];
    volatile uint8_t        rx_head;
    volatile uint8_t        rx_tail;
} can_cmsis_context_t;

static can_cmsis_context_t g_can_ctx[2] = {
    {
        DSPIC33AK_CANFD_INST_1,
        can1_msg_ram, (uint16_t)sizeof(can1_msg_ram),
        NULL, NULL,
        RTE_CAN1_CLK_HZ, RTE_CAN1_NOMINAL_BPS, RTE_CAN1_DATA_BPS,
        RTE_CAN1_SAMPLE_PCT, RTE_CAN1_FD_MODE, RTE_CAN1_IRQ_PRIORITY, RTE_CAN1_TIMEOUT_MS,
        RTE_CAN1, 0u, 0u,
        DSPIC33AK_CANFD_MODE_NORMAL_FD
    },
    {
        DSPIC33AK_CANFD_INST_2,
        can2_msg_ram, (uint16_t)sizeof(can2_msg_ram),
        NULL, NULL,
        RTE_CAN2_CLK_HZ, RTE_CAN2_NOMINAL_BPS, RTE_CAN2_DATA_BPS,
        RTE_CAN2_SAMPLE_PCT, RTE_CAN2_FD_MODE, RTE_CAN2_IRQ_PRIORITY, RTE_CAN2_TIMEOUT_MS,
        RTE_CAN2, 0u, 0u,
        DSPIC33AK_CANFD_MODE_NORMAL_FD
    }
};

/* ========================================================================== */
/* Tick provider (weak; override in the application)                          */
/* ========================================================================== */

__attribute__((weak))
uint32_t Driver_CAN_dsPIC33AK_GetMs(void)
{
    return 0u;
}

/* ========================================================================== */
/* Helpers                                                                    */
/* ========================================================================== */

static int32_t can_hal_to_arm(dspic33ak_canfd_status_t st)
{
    switch (st) {
    case DSPIC33AK_CANFD_OK:                  return ARM_DRIVER_OK;
    case DSPIC33AK_CANFD_ERR_INVALID_ARG:     return ARM_DRIVER_ERROR_PARAMETER;
    case DSPIC33AK_CANFD_ERR_BUSY:            return ARM_DRIVER_ERROR_BUSY;
    case DSPIC33AK_CANFD_ERR_UNSUPPORTED:     return ARM_DRIVER_ERROR_UNSUPPORTED;
    case DSPIC33AK_CANFD_ERR_NOT_INITIALIZED: return ARM_DRIVER_ERROR;
    case DSPIC33AK_CANFD_ERR_TIMEOUT:         return ARM_DRIVER_ERROR_TIMEOUT;
    default:                                  return ARM_DRIVER_ERROR;
    }
}

/* Map an ARM CAN mode to a HAL mode. Returns false for unsupported modes. */
static bool can_map_mode(ARM_CAN_MODE arm_mode, dspic33ak_canfd_mode_t *hal_mode)
{
    switch (arm_mode) {
    case ARM_CAN_MODE_NORMAL:            *hal_mode = DSPIC33AK_CANFD_MODE_NORMAL_FD;        return true;
    case ARM_CAN_MODE_MONITOR:           *hal_mode = DSPIC33AK_CANFD_MODE_LISTEN_ONLY;      return true;
    case ARM_CAN_MODE_LOOPBACK_INTERNAL: *hal_mode = DSPIC33AK_CANFD_MODE_INTERNAL_LOOPBACK; return true;
    case ARM_CAN_MODE_LOOPBACK_EXTERNAL: *hal_mode = DSPIC33AK_CANFD_MODE_EXTERNAL_LOOPBACK; return true;
    default:                             return false;   /* INITIALIZATION / RESTRICTED */
    }
}

/* (Re)apply HAL init for the context's current settings + the given mode, then
 * register the event callback and enable RX/error interrupts. */
static int32_t can_apply_init(can_cmsis_context_t *ctx, dspic33ak_canfd_mode_t mode);

/* Shared HAL event callback (CAN ISR context). user_data is the instance ctx. */
static void can_hal_event(dspic33ak_canfd_instance_t inst, uint32_t events, void *user_data)
{
    can_cmsis_context_t *ctx = (can_cmsis_context_t *)user_data;

    (void)inst;
    if (ctx == NULL) {
        return;
    }

    if ((events & DSPIC33AK_CANFD_EVENT_RX_AVAILABLE) != 0u) {
        dspic33ak_canfd_frame_t f;
        bool received = false;
        bool dropped  = false;

        /* Drain the hardware RX FIFO into the ring HERE (ISR context) so the RX
         * interrupt does not re-assert; MessageRead later pops from the ring. */
        while (dspic33ak_canfd_rx_available(inst)) {
            uint8_t nh;
            if (dspic33ak_canfd_receive(inst, &f) != DSPIC33AK_CANFD_OK) {
                break;
            }
            nh = (uint8_t)((ctx->rx_head + 1u) % CAN_RX_RING_LEN);
            if (nh != ctx->rx_tail) {
                ctx->rx_ring[ctx->rx_head] = f;
                ctx->rx_head = nh;
                received = true;
            } else {
                dropped = true;   /* ring full */
            }
        }
        if (received && (ctx->cb_object != NULL)) {
            ctx->cb_object(CAN_OBJ_RX, ARM_CAN_EVENT_RECEIVE);
        }
        if (dropped && (ctx->cb_object != NULL)) {
            ctx->cb_object(CAN_OBJ_RX, ARM_CAN_EVENT_RECEIVE_OVERRUN);
        }
    }
    if ((events & DSPIC33AK_CANFD_EVENT_RX_OVERFLOW) != 0u) {
        if (ctx->cb_object != NULL) {
            ctx->cb_object(CAN_OBJ_RX, ARM_CAN_EVENT_RECEIVE_OVERRUN);
        }
    }
    if ((events & DSPIC33AK_CANFD_EVENT_BUS_OFF) != 0u) {
        if (ctx->cb_unit != NULL) {
            ctx->cb_unit(ARM_CAN_EVENT_UNIT_BUS_OFF);
        }
    }
}

static int32_t can_apply_init(can_cmsis_context_t *ctx, dspic33ak_canfd_mode_t mode)
{
    dspic33ak_canfd_config_t cfg;
    dspic33ak_canfd_status_t st;

    cfg.can_clk_hz   = ctx->can_clk_hz;
    cfg.nominal_bps  = ctx->nominal_bps;
    cfg.data_bps     = ctx->data_bps;
    cfg.sample_pct   = ctx->sample_pct;
    cfg.brs          = (ctx->fd_mode != 0u);
    cfg.mode         = mode;
    cfg.timeout_ms   = ctx->timeout_ms;
    cfg.get_ms       = Driver_CAN_dsPIC33AK_GetMs;
    cfg.msg_ram      = ctx->msg_ram;
    cfg.msg_ram_size = ctx->msg_ram_size;

    st = dspic33ak_canfd_init(ctx->hal_inst, &cfg);
    if (st != DSPIC33AK_CANFD_OK) {
        return can_hal_to_arm(st);
    }

    (void)dspic33ak_canfd_isr_set_callback(ctx->hal_inst, can_hal_event, ctx);
    st = dspic33ak_canfd_isr_enable(ctx->hal_inst, ctx->irq_priority);
    if (st != DSPIC33AK_CANFD_OK) {
        return can_hal_to_arm(st);
    }

    ctx->mode = mode;
    return ARM_DRIVER_OK;
}

/* ========================================================================== */
/* CMSIS CAN driver functions (index-based core)                              */
/* ========================================================================== */

static ARM_DRIVER_VERSION CAN_GetVersion(void)
{
    return can_driver_version;
}

static ARM_CAN_CAPABILITIES CAN_GetCapabilities(void)
{
    return can_capabilities;
}

static int32_t CAN_Initialize(uint32_t index,
                              ARM_CAN_SignalUnitEvent_t   cb_unit_event,
                              ARM_CAN_SignalObjectEvent_t cb_object_event)
{
    can_cmsis_context_t *ctx = &g_can_ctx[index];

    ctx->cb_unit     = cb_unit_event;
    ctx->cb_object   = cb_object_event;
    ctx->powered     = 0u;
    ctx->initialized = 1u;
    return ARM_DRIVER_OK;
}

static int32_t CAN_PowerControl(uint32_t index, ARM_POWER_STATE state);  /* fwd */

static int32_t CAN_Uninitialize(uint32_t index)
{
    can_cmsis_context_t *ctx = &g_can_ctx[index];

    if (ctx->powered) {
        (void)CAN_PowerControl(index, ARM_POWER_OFF);
    }
    ctx->cb_unit     = NULL;
    ctx->cb_object   = NULL;
    ctx->initialized = 0u;
    return ARM_DRIVER_OK;
}

static int32_t CAN_PowerControl(uint32_t index, ARM_POWER_STATE state)
{
    can_cmsis_context_t *ctx = &g_can_ctx[index];
    int32_t rc;

    if (!ctx->initialized) {
        return ARM_DRIVER_ERROR;
    }

    switch (state) {
    case ARM_POWER_FULL:
        if (ctx->powered) {
            return ARM_DRIVER_OK;
        }
        ctx->rx_head = 0u;
        ctx->rx_tail = 0u;
        /* Board bring-up (PMD/clock/PPS/STBY) must already be done by the app. */
        rc = can_apply_init(ctx, DSPIC33AK_CANFD_MODE_NORMAL_FD);
        if (rc != ARM_DRIVER_OK) {
            return rc;
        }
        ctx->powered = 1u;
        return ARM_DRIVER_OK;

    case ARM_POWER_OFF:
        if (ctx->powered) {
            (void)dspic33ak_canfd_isr_disable(ctx->hal_inst);
            (void)dspic33ak_canfd_isr_set_callback(ctx->hal_inst, NULL, NULL);
            (void)dspic33ak_canfd_deinit(ctx->hal_inst);
        }
        ctx->powered = 0u;
        return ARM_DRIVER_OK;

    case ARM_POWER_LOW:
        return ARM_DRIVER_ERROR_UNSUPPORTED;

    default:
        return ARM_DRIVER_ERROR_PARAMETER;
    }
}

static uint32_t CAN_GetClock(uint32_t index)
{
    return g_can_ctx[index].can_clk_hz;
}

static int32_t CAN_SetBitrate(uint32_t index,
                              ARM_CAN_BITRATE_SELECT select,
                              uint32_t bitrate,
                              uint32_t bit_segments)
{
    can_cmsis_context_t *ctx = &g_can_ctx[index];

    if (bitrate == 0u) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }

    /* Optionally derive the sample point from the ARM bit-segment fields; the HAL
     * computes the concrete TSEG distribution from (bit rate, sample %). */
    if (bit_segments != 0u) {
        uint32_t prop = (bit_segments & ARM_CAN_BIT_PROP_SEG_Msk)   >> ARM_CAN_BIT_PROP_SEG_Pos;
        uint32_t ph1  = (bit_segments & ARM_CAN_BIT_PHASE_SEG1_Msk) >> ARM_CAN_BIT_PHASE_SEG1_Pos;
        uint32_t ph2  = (bit_segments & ARM_CAN_BIT_PHASE_SEG2_Msk) >> ARM_CAN_BIT_PHASE_SEG2_Pos;
        uint32_t tq   = 1u + prop + ph1 + ph2;
        if ((ph2 != 0u) && (tq > 1u)) {
            ctx->sample_pct = (uint8_t)(((1u + prop + ph1) * 100u) / tq);
        }
    }

    switch (select) {
    case ARM_CAN_BITRATE_NOMINAL: ctx->nominal_bps = bitrate; break;
    case ARM_CAN_BITRATE_FD_DATA: ctx->data_bps    = bitrate; break;
    default:                      return ARM_DRIVER_ERROR_PARAMETER;
    }

    if (!ctx->powered) {
        return ARM_DRIVER_OK;   /* cached; applied at PowerControl(FULL) */
    }
    return can_hal_to_arm(dspic33ak_canfd_set_bitrate(ctx->hal_inst, ctx->can_clk_hz,
                                                      ctx->nominal_bps, ctx->data_bps,
                                                      ctx->sample_pct));
}

static int32_t CAN_SetMode(uint32_t index, ARM_CAN_MODE mode)
{
    can_cmsis_context_t *ctx = &g_can_ctx[index];
    dspic33ak_canfd_mode_t hal_mode;

    if (!ctx->initialized) {
        return ARM_DRIVER_ERROR;
    }

    /* INITIALIZATION is the configured-but-not-live state; this wrapper performs
     * the actual bring-up in PowerControl(FULL), so treat it as a no-op. */
    if (mode == ARM_CAN_MODE_INITIALIZATION) {
        return ARM_DRIVER_OK;
    }
    if (!can_map_mode(mode, &hal_mode)) {
        return ARM_DRIVER_ERROR_UNSUPPORTED;   /* RESTRICTED etc. */
    }
    if (!ctx->powered) {
        return ARM_DRIVER_ERROR;
    }
    if (hal_mode == ctx->mode) {
        return ARM_DRIVER_OK;
    }
    /* Mode change re-applies the configuration in the new mode. */
    return can_apply_init(ctx, hal_mode);
}

static ARM_CAN_OBJ_CAPABILITIES CAN_ObjectGetCapabilities(uint32_t index, uint32_t obj_idx)
{
    ARM_CAN_OBJ_CAPABILITIES caps = {0};

    (void)index;
    if (obj_idx == CAN_OBJ_TX) {
        caps.tx           = 1u;
        caps.message_depth = 4u;   /* TX queue depth */
    } else if (obj_idx == CAN_OBJ_RX) {
        caps.rx           = 1u;
        caps.message_depth = 4u;   /* RX FIFO 1 depth */
    }
    return caps;
}

static int32_t CAN_ObjectSetFilter(uint32_t index, uint32_t obj_idx,
                                   ARM_CAN_FILTER_OPERATION operation,
                                   uint32_t id, uint32_t arg)
{
    (void)index; (void)obj_idx; (void)operation; (void)id; (void)arg;
    /* Initial scope: the RX object accepts all identifiers; configurable
     * exact/range/mask filters are not yet implemented. */
    return ARM_DRIVER_ERROR_UNSUPPORTED;
}

static int32_t CAN_ObjectConfigure(uint32_t index, uint32_t obj_idx, ARM_CAN_OBJ_CONFIG obj_cfg)
{
    (void)index;

    /* Fixed object model: obj 0 is TX, obj 1 is RX (both already set up by init).
     * Accept the matching/inactive configuration, reject anything else. */
    switch (obj_cfg) {
    case ARM_CAN_OBJ_INACTIVE:
        return ARM_DRIVER_OK;
    case ARM_CAN_OBJ_TX:
        return (obj_idx == CAN_OBJ_TX) ? ARM_DRIVER_OK : ARM_DRIVER_ERROR_UNSUPPORTED;
    case ARM_CAN_OBJ_RX:
        return (obj_idx == CAN_OBJ_RX) ? ARM_DRIVER_OK : ARM_DRIVER_ERROR_UNSUPPORTED;
    default:
        return ARM_DRIVER_ERROR_UNSUPPORTED;   /* RTR auto-data objects */
    }
}

static int32_t CAN_MessageSend(uint32_t index, uint32_t obj_idx,
                               ARM_CAN_MSG_INFO *msg_info, const uint8_t *data, uint8_t size)
{
    can_cmsis_context_t *ctx = &g_can_ctx[index];
    dspic33ak_canfd_frame_t frame;
    dspic33ak_canfd_status_t st;
    uint8_t i;

    if (!ctx->powered) {
        return ARM_DRIVER_ERROR;
    }
    if ((msg_info == NULL) || (obj_idx != CAN_OBJ_TX) || (size > 64u) ||
        ((size > 0u) && (data == NULL))) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }

    frame.extended = ((msg_info->id & ARM_CAN_ID_IDE_Msk) != 0u);
    frame.id       = frame.extended ? (msg_info->id & 0x1FFFFFFFu) : (msg_info->id & 0x7FFu);
    frame.fd       = (msg_info->edl != 0u);
    frame.brs      = (msg_info->brs != 0u);
    frame.len      = size;
    for (i = 0u; i < size; i++) {
        frame.data[i] = data[i];
    }

    st = dspic33ak_canfd_transmit(ctx->hal_inst, &frame);
    if (st != DSPIC33AK_CANFD_OK) {
        return can_hal_to_arm(st);
    }
    return (int32_t)size;   /* bytes accepted */
}

static int32_t CAN_MessageRead(uint32_t index, uint32_t obj_idx,
                               ARM_CAN_MSG_INFO *msg_info, uint8_t *data, uint8_t size)
{
    can_cmsis_context_t *ctx = &g_can_ctx[index];
    dspic33ak_canfd_frame_t frame;
    uint8_t n, i;

    if (!ctx->powered) {
        return ARM_DRIVER_ERROR;
    }
    if ((msg_info == NULL) || (obj_idx != CAN_OBJ_RX) || ((size > 0u) && (data == NULL))) {
        return ARM_DRIVER_ERROR_PARAMETER;
    }

    /* Pop one frame from the RX ring (filled by the ISR). */
    if (ctx->rx_tail == ctx->rx_head) {
        return ARM_DRIVER_ERROR;   /* no message available */
    }
    frame = ctx->rx_ring[ctx->rx_tail];
    ctx->rx_tail = (uint8_t)((ctx->rx_tail + 1u) % CAN_RX_RING_LEN);

    msg_info->id  = frame.extended ? ((frame.id & 0x1FFFFFFFu) | ARM_CAN_ID_IDE_Msk)
                                   : (frame.id & 0x7FFu);
    msg_info->rtr = 0u;
    msg_info->edl = frame.fd ? 1u : 0u;
    msg_info->brs = frame.brs ? 1u : 0u;
    msg_info->esi = 0u;
    msg_info->dlc = (frame.len <= 8u) ? frame.len :
                    (frame.len <= 12u) ? 9u : (frame.len <= 16u) ? 10u :
                    (frame.len <= 20u) ? 11u : (frame.len <= 24u) ? 12u :
                    (frame.len <= 32u) ? 13u : (frame.len <= 48u) ? 14u : 15u;

    n = (frame.len < size) ? frame.len : size;
    for (i = 0u; i < n; i++) {
        data[i] = frame.data[i];
    }
    return (int32_t)n;   /* bytes read */
}

static int32_t CAN_Control(uint32_t index, uint32_t control, uint32_t arg)
{
    can_cmsis_context_t *ctx = &g_can_ctx[index];

    if (!ctx->initialized) {
        return ARM_DRIVER_ERROR;
    }

    switch (control & ARM_CAN_CONTROL_Msk) {
    case ARM_CAN_SET_FD_MODE:
        ctx->fd_mode = (arg != 0u) ? 1u : 0u;
        if (ctx->powered) {
            return can_apply_init(ctx, ctx->mode);   /* re-apply with new FD setting */
        }
        return ARM_DRIVER_OK;

    case ARM_CAN_ABORT_MESSAGE_SEND:
        if (!ctx->powered) {
            return ARM_DRIVER_ERROR;
        }
        return can_hal_to_arm(dspic33ak_canfd_tx_abort(ctx->hal_inst));

    case ARM_CAN_CONTROL_RETRANSMISSION:
    case ARM_CAN_SET_TRANSCEIVER_DELAY:   /* TDC is automatic in the HAL */
    default:
        return ARM_DRIVER_ERROR_UNSUPPORTED;
    }
}

static ARM_CAN_STATUS CAN_GetStatus(uint32_t index)
{
    can_cmsis_context_t *ctx = &g_can_ctx[index];
    ARM_CAN_STATUS status = {0};
    dspic33ak_canfd_bus_status_t bs = {0};

    if (!ctx->powered) {
        status.unit_state = ARM_CAN_UNIT_STATE_INACTIVE;
        return status;
    }
    if (dspic33ak_canfd_get_status(ctx->hal_inst, &bs) != DSPIC33AK_CANFD_OK) {
        status.unit_state = ARM_CAN_UNIT_STATE_INACTIVE;
        return status;
    }

    if (bs.bus_off) {
        status.unit_state = ARM_CAN_UNIT_STATE_BUS_OFF;
    } else if (bs.error_passive) {
        status.unit_state = ARM_CAN_UNIT_STATE_PASSIVE;
    } else {
        status.unit_state = ARM_CAN_UNIT_STATE_ACTIVE;
    }
    status.last_error_code = ARM_CAN_LEC_NO_ERROR;   /* not tracked in v0.1 */
    status.tx_error_count  = bs.tx_err_count;
    status.rx_error_count  = bs.rx_err_count;
    return status;
}

/* ========================================================================== */
/* Per-instance access structures                                             */
/* ========================================================================== */

#define CAN_DRIVER_WRAPPERS(drv, idx)                                                              \
    static int32_t CAN##drv##_Initialize(ARM_CAN_SignalUnitEvent_t u, ARM_CAN_SignalObjectEvent_t o) \
    { return CAN_Initialize((idx), u, o); }                                                        \
    static int32_t CAN##drv##_Uninitialize(void)                                                   \
    { return CAN_Uninitialize((idx)); }                                                            \
    static int32_t CAN##drv##_PowerControl(ARM_POWER_STATE state)                                  \
    { return CAN_PowerControl((idx), state); }                                                     \
    static uint32_t CAN##drv##_GetClock(void)                                                      \
    { return CAN_GetClock((idx)); }                                                                \
    static int32_t CAN##drv##_SetBitrate(ARM_CAN_BITRATE_SELECT sel, uint32_t br, uint32_t seg)    \
    { return CAN_SetBitrate((idx), sel, br, seg); }                                                \
    static int32_t CAN##drv##_SetMode(ARM_CAN_MODE mode)                                           \
    { return CAN_SetMode((idx), mode); }                                                           \
    static ARM_CAN_OBJ_CAPABILITIES CAN##drv##_ObjectGetCapabilities(uint32_t obj)                 \
    { return CAN_ObjectGetCapabilities((idx), obj); }                                              \
    static int32_t CAN##drv##_ObjectSetFilter(uint32_t obj, ARM_CAN_FILTER_OPERATION op,           \
                                              uint32_t id, uint32_t arg)                           \
    { return CAN_ObjectSetFilter((idx), obj, op, id, arg); }                                       \
    static int32_t CAN##drv##_ObjectConfigure(uint32_t obj, ARM_CAN_OBJ_CONFIG cfg)                \
    { return CAN_ObjectConfigure((idx), obj, cfg); }                                               \
    static int32_t CAN##drv##_MessageSend(uint32_t obj, ARM_CAN_MSG_INFO *mi,                      \
                                          const uint8_t *d, uint8_t sz)                            \
    { return CAN_MessageSend((idx), obj, mi, d, sz); }                                             \
    static int32_t CAN##drv##_MessageRead(uint32_t obj, ARM_CAN_MSG_INFO *mi,                      \
                                          uint8_t *d, uint8_t sz)                                  \
    { return CAN_MessageRead((idx), obj, mi, d, sz); }                                             \
    static int32_t CAN##drv##_Control(uint32_t control, uint32_t arg)                              \
    { return CAN_Control((idx), control, arg); }                                                   \
    static ARM_CAN_STATUS CAN##drv##_GetStatus(void)                                               \
    { return CAN_GetStatus((idx)); }

CAN_DRIVER_WRAPPERS(1, 0)
CAN_DRIVER_WRAPPERS(2, 1)

ARM_DRIVER_CAN Driver_CAN1 = {
    CAN_GetVersion,
    CAN_GetCapabilities,
    CAN1_Initialize,
    CAN1_Uninitialize,
    CAN1_PowerControl,
    CAN1_GetClock,
    CAN1_SetBitrate,
    CAN1_SetMode,
    CAN1_ObjectGetCapabilities,
    CAN1_ObjectSetFilter,
    CAN1_ObjectConfigure,
    CAN1_MessageSend,
    CAN1_MessageRead,
    CAN1_Control,
    CAN1_GetStatus
};

ARM_DRIVER_CAN Driver_CAN2 = {
    CAN_GetVersion,
    CAN_GetCapabilities,
    CAN2_Initialize,
    CAN2_Uninitialize,
    CAN2_PowerControl,
    CAN2_GetClock,
    CAN2_SetBitrate,
    CAN2_SetMode,
    CAN2_ObjectGetCapabilities,
    CAN2_ObjectSetFilter,
    CAN2_ObjectConfigure,
    CAN2_MessageSend,
    CAN2_MessageRead,
    CAN2_Control,
    CAN2_GetStatus
};
