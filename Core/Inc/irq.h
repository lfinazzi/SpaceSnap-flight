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
 * @brief  DCMI global interrupt handler.
 *
 * @note   Handles DCMI peripheral interrupts including frame capture complete,
 *         VSYNC events, line events, and FIFO overrun errors. Delegates
 *         entirely to HAL_DCMI_IRQHandler() which in turn fires the
 *         appropriate HAL callbacks:
 *           HAL_DCMI_FrameEventCallback() on frame complete
 *           HAL_DCMI_ErrorCallback()      on FIFO overrun or sync error
 *         Both callbacks are overridden in photo.c to set dcmi_frame_ready
 *         and dcmi_error flags respectively, which are polled by
 *         Photo_CaptureRaw() in the main execution context.
 ********************************************************************************/
void DCMI_IRQHandler(void);

/********************************************************************************
 * @brief  DMA2 Stream1 global interrupt handler.
 *
 * @note   Handles DMA transfer interrupts for the DCMI peripheral. DMA2
 *         Stream1 is configured to transfer 32-bit words from the DCMI
 *         data register directly to the target raw_photo_t.data[] buffer
 *         in external SRAM via FSMC. Delegates entirely to
 *         HAL_DMA_IRQHandler() which manages half-transfer, transfer
 *         complete and transfer error events. Transfer complete triggers
 *         HAL_DCMI_FrameEventCallback() via the DCMI-DMA linkage
 *         established by __HAL_LINKDMA() during initialisation.
 ********************************************************************************/
void DMA2_Stream1_IRQHandler(void);

/********************************************************************************
 * @brief  HAL callback fired when DMA has transferred a complete frame
 *         into the target SRAM buffer.
 *
 * @note   Overrides the HAL weak definition. Sets dcmi_frame_ready = 1 to
 *         signal Photo_CaptureRaw() that the frame is complete and the
 *         raw_photo_t.data[] buffer in SRAM is valid and ready for
 *         processing or compression. Called from DMA2_Stream1_IRQHandler
 *         via the HAL DMA-DCMI linkage. Keep ISR body minimal — no
 *         blocking calls, no HAL delays, no UART logging.
 *
 * @param  hdcmi   Pointer to the DCMI handle (hdcmi, defined in main.c).
 ********************************************************************************/
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi);

/* Flags set by DCMI callbacks, polled by Photo_CaptureRaw()          */
extern volatile uint8_t dcmi_frame_ready;   // 1 = frame complete in SRAM
extern volatile uint8_t dcmi_error;         // 1 = FIFO overrun or sync error


#endif
