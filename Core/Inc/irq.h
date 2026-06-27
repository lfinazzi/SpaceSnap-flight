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


/********************************************************************************
 * @brief  HAL weak override — called on any DCMI capture error.
 *
 * @note   Overrides the HAL weak definition. Sets dcmi_error = 1 to signal
 *         Photo_CaptureRaw() that a capture fault occurred before the frame
 *         completed. The error source can be read from hdcmi->ErrorCode
 *         after the callback returns; possible values are:
 *           HAL_DCMI_ERROR_OVR     — FIFO overrun (data arrived faster than
 *                                    DMA could drain it)
 *           HAL_DCMI_ERROR_SYNC    — Embedded synchronisation mismatch
 *                                    (unexpected SAV/EAV codes)
 *           HAL_DCMI_ERROR_DMA     — DMA transfer fault
 *           HAL_DCMI_ERROR_TIMEOUT — Capture timeout
 *
 *         Photo_CaptureRaw() polls dcmi_error in the same loop as
 *         dcmi_frame_ready; a set dcmi_error causes the loop to exit
 *         immediately rather than waiting for DCMI_TIMEOUT to expire.
 *         This allows a meaningful distinction between a capture error
 *         and a plain timeout.
 *
 *         Called from DCMI_IRQHandler via the HAL DCMI interrupt service
 *         routine — do not call Log() or any other blocking function from
 *         this context.
 *
 * @param  hdcmi  Pointer to the DCMI handle that detected the error.
 ********************************************************************************/
void HAL_DCMI_ErrorCallback(DCMI_HandleTypeDef *hdcmi);


/* Flags set by DCMI callbacks, polled by Photo_CaptureRaw()          */
extern volatile uint8_t dcmi_frame_ready;   // 1 = frame complete in SRAM
extern volatile uint8_t dcmi_error;         // 1 = FIFO overrun or sync error


#endif
