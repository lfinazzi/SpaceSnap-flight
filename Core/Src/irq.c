#include "irq.h"

extern UART_HandleTypeDef huart1;

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	// Size = actual number of bytes received
	// rx_buffer contains the message
	rx_flag = 1;
	rx_size = Size;
	HAL_UARTEx_ReceiveToIdle_IT(&huart1, (uint8_t*) rx_buffer, AIRMAC_SIZE+1); // re-arm
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
	if (GPIO_Pin == GPIO_PIN_8)			// PA8
	{
		uss_comm_reset = 1; 			// Received GPIO reset from LS-02
	}
}
