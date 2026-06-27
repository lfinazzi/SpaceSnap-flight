/**
  ******************************************************************************
  * @file           : telemetry.h
  * @brief          : Telemetry interface — response frame encoding declarations
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#ifndef __TELEMETRY_H__
#define __TELEMETRY_H__

#define TEMPSENSOR_V25         				0.76f    // V, from Table 68 in STM32F2xx reference guide (typ.)
#define TEMPSENSOR_AVG_SLOPE   				2.5f     // mV/°C, from Table 68 in STM32F2xx reference guide (typ.)
#define VREF_VOLTAGE           				3.3f
#define ADC_MAX_VALUE          				4095.0f
#define VREFINT_CAL							1.21f	// VREFINT, from table 70 in STM32Fxx reference guide (typ.)


/********************************************************************************
 * @brief  Checks RCC reset-cause flags on bootup and updates board_status
 *         reset counters and last_reset_cause accordingly.
 *
 * @note   Must be called once early in main() after HAL_Init() and clock
 *         configuration, before any other logic that modifies board_status.
 *         Checks RCC flags in priority order (IWDG → LPWR → SFT → POR →
 *         PIN → unknown) and increments the corresponding counter in
 *         board_status. Sets board_status.last_reset_cause to the matching
 *         RESET_CAUSE_* code. Clears all RCC reset flags via
 *         __HAL_RCC_CLEAR_RESET_FLAGS() after reading to prevent stale
 *         flags from persisting into the next reset cycle.
 *
 *         Note: RCC_FLAG_PINRST may be set alongside other flags on some
 *         STM32F2 reset paths. The priority order above ensures more
 *         specific flags are matched first. Verify flag behavior against
 *         your reference manual if reset cause attribution seems incorrect.
 *
 *         Does not call SaveBoardStatusFRAM() -- the caller is responsible
 *         for persisting board_status after this function returns.
 ********************************************************************************/
void CheckResetCause(void);


/********************************************************************************
 * @brief  Reads MCU internal temperature sensor and VREFINT via ADC1 and
 *         stores the raw 12-bit codes into board_status.
 *
 * @note   Performs a two-channel sequential scan on ADC1. Temperature sensor
 *         is rank 1 (read first), VREFINT is rank 2 (read second). Raw ADC
 *         codes (0–4095) are stored directly into board_status.mcu_temp and
 *         board_status.vrefint without conversion. Decoding to degrees Celsius
 *         and volts is deferred to LogBoardStatusFull() using TEMPSENSOR_V25,
 *         TEMPSENSOR_AVG_SLOPE, and VREFINT_CAL constants.
 *
 *         ADC1 must be configured in scan mode with both channels enabled
 *         and EOC set to fire at the end of each individual conversion
 *         (not end of sequence) for HAL_ADC_PollForConversion() to work
 *         correctly between the two reads. Configure via .ioc before using
 *         this function.
 *
 *         Called automatically by UpdateStatus().
 ********************************************************************************/
void Read_MCU_ADC_Vals(void);


#endif	/* __TELEMETRY_H__ */
