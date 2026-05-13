#ifndef __IRQ_H__
#define __IRQ_H__

#include <stdint.h>
#include "main.h"

/********************************************************************************
 * @brief  UART Rx Event callback, invoked by the HAL when a DMA/idle-line
 *         reception event completes on the given UART peripheral.
 *
 * @note   Called in interrupt context. Triggered either when the DMA transfer
 *         reaches the programmed length OR when an idle-line event is detected
 *         (i.e. the bus goes silent before the buffer is full). This makes it
 *         suitable for variable-length frame reception without a fixed timeout.
 *
 * @param  huart  Pointer to the UART_HandleTypeDef for the peripheral that
 *                triggered the event. Check huart->Instance to demux between
 *                multiple UARTs (e.g. huart1, huart2).
 * @param  Size   Number of bytes actually received and written into the DMA
 *                buffer since the last transfer start. Use this value instead
 *                of the originally requested length when processing the frame.
 *********************************************************************************/
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);


/********************************************************************************
 * @brief  GPIO EXTI (external interrupt) callback, invoked by the HAL when a
 *         configured edge event fires on a GPIO pin.
 *
 * @note   Called in interrupt context from the EXTIx_IRQHandler after the HAL
 *         clears the pending flag. All EXTI lines that share an IRQ vector
 *         (e.g. EXTI9_5 or EXTI15_10 on STM32) funnel through this single
 *         callback, so GPIO_Pin must always be checked before acting.
 *         Keep the body short; defer heavy work to a task via a FreeRTOS
 *         semaphore or task notification.
 *
 * @param  GPIO_Pin  Bitmask identifying the pin that triggered the interrupt
 *                   (e.g. GPIO_PIN_3). Compare with the expected pin constant
 *                   rather than assuming only one pin fires at a time.
 *********************************************************************************/
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

#endif
