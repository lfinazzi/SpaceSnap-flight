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



#endif
