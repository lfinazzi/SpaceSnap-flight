/**
  ******************************************************************************
  * @file           : irq.c
  * @brief          : Interrupt handlers — UART and DCMI callback implementations
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#include "irq.h"
#include "comms.h"
#include "photo.h"
#include "main.h"

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
	// Size = actual number of bytes received
	// rx_buffer contains the message
	rx_flag = 1;
	rx_size = Size;
	HAL_UARTEx_ReceiveToIdle_IT(&huart1, (uint8_t*) rx_buffer, AIRMAC_SIZE+1); // re-arm
}

/* ------------------------------------------------------------------ */
/*  Called by HAL when DMA has transferred a complete frame to SRAM   */
/* ------------------------------------------------------------------ */
void HAL_DCMI_FrameEventCallback(DCMI_HandleTypeDef *hdcmi)
{
    dcmi_frame_ready = 1;
}

void HAL_DCMI_ErrorCallback(DCMI_HandleTypeDef *hdcmi)
{
    dcmi_error = 1;
}
