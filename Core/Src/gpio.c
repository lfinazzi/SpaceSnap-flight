/**
  ******************************************************************************
  * @file           : gpio.c
  * @brief          : GPIO initialization
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#include "gpio.h"
#include "main.h"

void GPIO_Init(void)
{
	  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);			// Sets LS-02 reset GPIO (PC9) to low, low on startup

	  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);			// RS-485 RE, low on startup (enabled)
	  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);			// RS-485 DE, low on startup (disabled)

	  // All enables off on startup

	  // IMG_I2C_ENA
	  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);

	  // IMG_ENA
	  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);

	  // IMG_ENA_A
	  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);

	  // IMG_ENA_B
	  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);

	  // RESET_BAR, shared by two cameras!
	  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);		// only turn on after power rails have stabilized!

	  // UP, LB low allows SRAM to operate in 16b mode

	  // MEMO_UB, low on startup
	  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_12, GPIO_PIN_RESET);

	  // MEMO_LB, low on startup
	  HAL_GPIO_WritePin(GPIOG, GPIO_PIN_13, GPIO_PIN_RESET);

}
