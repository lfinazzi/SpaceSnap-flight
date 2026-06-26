/**
  ******************************************************************************
  * @file           : telemetry.c
  * @brief          : Telemetry encoder — packs board state into RS-485 response frames
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#include "telemetry.h"
#include "status.h"

extern board_status_t board_status;
extern ADC_HandleTypeDef hadc1;


void CheckResetCause(void)
{
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST)) {
        board_status.iwdg_reset_count++;
        board_status.last_reset_cause = RESET_CAUSE_IWDG;
        Log("Reset cause: IWDG timeout\r\n");
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST)) {
        board_status.lowpwr_reset_count++;
        board_status.last_reset_cause = RESET_CAUSE_LOWPOWER;
        Log("Reset cause: Low power\r\n");
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST)) {
    	board_status.sftw_reset_count++;
        board_status.last_reset_cause = RESET_CAUSE_SOFTWARE;
        Log("Reset cause: Software reset\r\n");
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST)) {
    	board_status.por_reset_count++;
        board_status.last_reset_cause = RESET_CAUSE_POR;
        Log("Reset cause: Power-on reset\r\n");
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST)) {
    	board_status.pin_reset_count++;
        board_status.last_reset_cause = RESET_CAUSE_PIN;
        Log("Reset cause: External reset pin\r\n");
    }
    else {
    	board_status.unk_reset_count++;
        board_status.last_reset_cause = RESET_CAUSE_UNKNOWN;
        Log("Reset cause: Unknown\r\n");
    }

    __HAL_RCC_CLEAR_RESET_FLAGS();
}

void Read_MCU_ADC_Vals(void)
{
	// Start ADC1 scan sequence
    HAL_ADC_Start(&hadc1);

    // Read temp sensor (rank 1)
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
    board_status.mcu_temp = HAL_ADC_GetValue(&hadc1);

    // Read VREFINT (rank 2)
    HAL_ADC_PollForConversion(&hadc1, HAL_MAX_DELAY);
    board_status.vrefint = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    return;
}


