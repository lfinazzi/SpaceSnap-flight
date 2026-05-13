#include "gpio.h"
#include "main.h"

void GPIO_Init(void)
{
	  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_9, GPIO_PIN_RESET);			// Sets LS-02 reset GPIO (PC9) to low
	  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);			// RS-485 RE high
	  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);			// RS-485 DE low

	  // TODO: all other GPIOs
}
