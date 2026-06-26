/**
  ******************************************************************************
  * @file           : irq.h
  * @brief          : IRQ interface — ISR-set volatile flag declarations
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#ifndef __IRQ_H__
#define __IRQ_H__

#include <stdint.h>
#include "main.h"


/********************************************************************************
 * @brief  UART Rx Event callback, invoked by the HAL when a DMA/idle-line
 *         reception completes on the given UART peripheral.
 *
 * @note   Called in interrupt context. Triggered either when the DMA transfer
 *         reaches the programmed length or when an idle-line event is detected
 *         (bus goes silent before the buffer is full), making it suitable for
 *         variable-length frame reception. Sets rx_flag and rx_size, then
 *         immediately re-arms HAL_UARTEx_ReceiveToIdle_IT() on huart1 for
 *         the next frame.
 *
 * @param  huart  Pointer to the UART handle that triggered the event.
 * @param  Size   Number of bytes actually received into rx_buffer.
 ********************************************************************************/
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);


/********************************************************************************
 * @brief  HAL callback fired when DMA has transferred a complete DCMI frame
 *         into the target SRAM buffer.
 *
 * @note   Overrides the HAL weak definition. Sets dcmi_frame_ready = 1 to
 *         signal Photo_CaptureRaw() that the frame is complete and
 *         raw_photo_t.data[] is valid and ready for processing or compression.
 *         Called from DMA2_Stream1_IRQHandler via the HAL DMA-DCMI linkage
 *         established by __HAL_LINKDMA() during initialization.
 *
 * @param  hdcmi  Pointer to the DCMI handle (hdcmi, defined in main.c).
 ********************************************************************************/
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi);

/* Flags set by DCMI callbacks, polled by Photo_CaptureRaw()          */
extern volatile uint8_t dcmi_frame_ready;   // 1 = frame complete in SRAM
extern volatile uint8_t dcmi_error;         // 1 = FIFO overrun or sync error


#endif
