#ifndef RTE_DEVICE_CAN_DSPIC33AK_EXAMPLE_H
#define RTE_DEVICE_CAN_DSPIC33AK_EXAMPLE_H

/*
 * Example RTE configuration for the dsPIC33AK CAN CMSIS-Driver wrapper.
 *
 * This is a CAN-only example configuration file.
 * It is not intended to be a shared application-level RTE_Device.h.
 * In an integrated application, copy the required CAN definitions into that
 * application's RTE_Device.h or equivalent configuration header.
 * Do not add I2C/USART/SPI/etc. settings to this CAN example file.
 *
 * The values below match the Perseus validation board: CAN clock 20 MHz (CLKGEN10
 * / PLL1, INTDIV=5, see osc_drv), nominal 500 kbit/s, data 2 Mbit/s, 80% sample
 * point. Adjust CLK_HZ / bit rates / IRQ priority to the target board.
 */

#define RTE_CAN1 1
#define RTE_CAN2 0

#define RTE_CAN1_CLK_HZ        20000000u   /* FCAN: CLKGEN10 / PLL1, INTDIV=5    */
#define RTE_CAN1_NOMINAL_BPS   500000u     /* arbitration-phase bit rate         */
#define RTE_CAN1_DATA_BPS      2000000u    /* FD data-phase bit rate             */
#define RTE_CAN1_SAMPLE_PCT    80u         /* sample point (%)                   */
#define RTE_CAN1_FD_MODE       1u          /* 1 = CAN FD (BRS) enabled           */
#define RTE_CAN1_TIMEOUT_MS    10u         /* HAL blocking-op timeout            */
#define RTE_CAN1_IRQ_PRIORITY  4u          /* CPU interrupt priority (1..7)      */

#define RTE_CAN2_CLK_HZ        20000000u
#define RTE_CAN2_NOMINAL_BPS   500000u
#define RTE_CAN2_DATA_BPS      2000000u
#define RTE_CAN2_SAMPLE_PCT    80u
#define RTE_CAN2_FD_MODE       1u
#define RTE_CAN2_TIMEOUT_MS    10u
#define RTE_CAN2_IRQ_PRIORITY  4u

#endif /* RTE_DEVICE_CAN_DSPIC33AK_EXAMPLE_H */
