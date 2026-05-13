#ifndef __GPIO_H__
#define __GPIO_H__

/********************************************************************************
 * @brief  Initializes GPIO output pins to their default states at startup.
 *
 * @note   Must be called once during system initialization, after MX_GPIO_Init().
 *         Sets the following initial states:
 *           - PC9  : LS-02 reset line — asserted LOW (device held in reset)
 *           - PB0  : RS-485 RE (Receiver Enable) — HIGH (receiver disabled)
 *           - PB1  : RS-485 DE (Driver Enable)   — LOW  (driver disabled)
 *
 * @ TODO   Extend with remaining GPIO initializations as peripherals are added.
 *********************************************************************************/
void GPIO_Init(void);


#endif
