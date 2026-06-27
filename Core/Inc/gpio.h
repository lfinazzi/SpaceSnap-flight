/**
  ******************************************************************************
  * @file           : gpio.h
  * @brief          : GPIO initialization
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#ifndef __GPIO_H__
#define __GPIO_H__


/********************************************************************************
 * @brief  Initializes GPIO output pins to their default states at startup.
 *
 * @note   Must be called once during system initialization, after MX_GPIO_Init().
 *         Sets the following initial states:
 *
 *  Pin    | Signal          | Init state | Description
 *  -------|-----------------|------------|--------------------------------------------
 *  PC9    | LS02_RESET      | LOW        | LS-02 reset line, held low on startup
 *  PB0    | RS485_RE        | LOW        | RS-485 receiver enable (active low = enabled)
 *  PB1    | RS485_DE        | LOW        | RS-485 driver enable (active high = disabled)
 *  PA11   | IMG_I2C_ENA     | LOW        | Camera I2C power enable, off on startup
 *  PA12   | IMG_ENA         | LOW        | Camera power enable, off on startup
 *  PA2    | IMG_ENA_A       | LOW        | Camera A enable, off on startup
 *  PA3    | IMG_ENA_B       | LOW        | Camera B enable, off on startup
 *  PC0    | CAM_RESET_BAR   | LOW        | Shared camera reset, only raise after
 *         |                 |            | power rails have stabilized
 *  PG12   | MEMO_UB         | LOW        | SRAM upper byte enable (low = active)
 *  PG13   | MEMO_LB         | LOW        | SRAM lower byte enable (low = 16b mode)
 *
 ********************************************************************************/
void GPIO_Init(void);


#endif
