#ifndef __TELEMETRY_H__
#define __TELEMETRY_H__

#define TEMPSENSOR_V25         				0.76f    // V, from Table 68 in STM32F2xx reference guide (typ.)
#define TEMPSENSOR_AVG_SLOPE   				2.5f     // mV/°C, from Table 68 in STM32F2xx reference guide (typ.)
#define VREF_VOLTAGE           				3.3f
#define ADC_MAX_VALUE          				4095.0f
#define VREFINT_CAL							1.21f	// VREFINT, from table 70 in STM32Fxx reference guide (typ.)

#include "stm32f2xx_hal.h"


// TODO: comment. Checks reset cause on startup
void CheckResetCause(void);

// TODO: comment. Reads Temp sensor and vrefint on MCU
void Read_MCU_ADC_Vals(void);


#endif
