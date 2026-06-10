#include "photo.h"
#include "comms.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c2;
extern TIM_HandleTypeDef htim11;
extern DCMI_HandleTypeDef hdcmi;
extern IWDG_HandleTypeDef hiwdg;

//DMA_HandleTypeDef hdma_dcmi;

volatile uint8_t dcmi_frame_ready 	= 0;   // set by callback, cleared by main
volatile uint8_t dcmi_error       	= 0;   // set on DCMI error

void CAM_ResetAssert(void)
{
    HAL_GPIO_WritePin(CAM_RESET_PORT, CAM_RESET_PIN, GPIO_PIN_RESET);
}

void CAM_ResetRelease(void)
{
    HAL_GPIO_WritePin(CAM_RESET_PORT, CAM_RESET_PIN, GPIO_PIN_SET);
}

/* ------------------------------------------------------------------ */
/*  Camera A full init sequence                                        */
/* ------------------------------------------------------------------ */
void ActivateCAMA(void)
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);  // IMG_ENA
	HAL_Delay(1);

	/* Step 1b: Start 27 MHz EXTCLK — must exist before rails come up */
	if (HAL_TIM_PWM_Start(&htim11, TIM_CHANNEL_1) != HAL_OK) {
		Log("EXTCLK start FAILED\r\n");
		Error_Handler();
	}
	HAL_Delay(1);   // let clock stabilise

	/* Step 1: Assert reset */
	CAM_ResetAssert();
	HAL_Delay(1);

	/* Step 2: Enable 2.8V rail */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_SET);   // IMG_ENA_A
	HAL_Delay(10);   // 2.8V rails stabilise

	/* Step 3: Enable I2C level shifter */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);  // IMG_I2C_ENA
	HAL_Delay(1);

	/* Step 4: Release reset
	 * Sensor internal regulator starts driving VREG_BASE → BSR16 → 1.8V */
	CAM_ResetRelease();

	/* Step 5: Wait for 1.8V to stabilise + firmware boot
	 * Internal regulator needs a few ms, firmware needs ~100ms */
	/* Wait for Auto-Config to complete — datasheet says ~100ms */
	HAL_Delay(150);

	Log("Camera A activated\r\n");

	uint16_t chip_id = 0;
	uint16_t state = 0;
	char log_buf[64];

	HAL_StatusTypeDef ret = CAM_ReadReg(CAM_I2C_ADDR_A, 0x3000, &chip_id);
	if(ret != HAL_OK)
	{
	    sprintf(log_buf, "Camera A I2C FAILED, ret=%d\r\n", ret);
	    Log(log_buf);
	    Error_Handler();
	}
	sprintf(log_buf, "Camera A chip_id: 0x%04X\r\n", chip_id);
	Log(log_buf);

	state = CAM_GetState(CAM_I2C_ADDR_A);
	if(state == 0xFFFF)
	{
	    Log("Camera A GetState FAILED\r\n");
	    Error_Handler();
	}
	sprintf(log_buf, "Camera A state before config: 0x%04X\r\n", state);
	Log(log_buf);
}


/* ------------------------------------------------------------------ */
/*  Camera B full init sequence                                        */
/* ------------------------------------------------------------------ */
void ActivateCAMB(void)
{
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);  // IMG_ENA
	HAL_Delay(1);

    /* Step 1b: Start 27 MHz EXTCLK — must exist before rails come up */
    if (HAL_TIM_PWM_Start(&htim11, TIM_CHANNEL_1) != HAL_OK) {
        Log("EXTCLK start FAILED\r\n");
        Error_Handler();
    }
    HAL_Delay(1);   // let clock stabilise

    /* Step 1: Assert reset */
    CAM_ResetAssert();
    HAL_Delay(1);

    /* Step 2: Enable 2.8V rail */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);   // IMG_ENA_B
    HAL_Delay(10);   // 2.8V rails stabilise

    /* Step 3: Enable I2C level shifter */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET);  // IMG_I2C_ENA
    HAL_Delay(1);

    /* Step 4: Release reset
     * Sensor internal regulator starts driving VREG_BASE → BSR16 → 1.8V */
    CAM_ResetRelease();

    /* Step 5: Wait for 1.8V to stabilise + firmware boot
     * Internal regulator needs a few ms, firmware needs ~100ms */
    /* Wait for Auto-Config to complete — datasheet says ~100ms */
    HAL_Delay(150);

    Log("Camera B activated\r\n");

    uint16_t chip_id = 0;
    uint16_t state = 0;
    char log_buf[64];

    HAL_StatusTypeDef ret = CAM_ReadReg(CAM_I2C_ADDR_B, 0x3000, &chip_id);
    if(ret != HAL_OK)
    {
        sprintf(log_buf, "Camera B I2C FAILED, ret=%d\r\n", ret);
        Log(log_buf);
        Error_Handler();
    }
    sprintf(log_buf, "Camera B chip_id: 0x%04X\r\n", chip_id);
    Log(log_buf);

    state = CAM_GetState(CAM_I2C_ADDR_B);
    if(state == 0xFFFF)
    {
        Log("Camera B GetState FAILED\r\n");
        Error_Handler();
    }
    sprintf(log_buf, "Camera B state before config: 0x%04X\r\n", state);
    Log(log_buf);
}

void DeactivateCAMA(void)
{
	/* Step 1: Assert reset first */
	CAM_ResetAssert();
	HAL_Delay(1);

	/* Step 2: Disable I2C level shifter */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);  // IMG_I2C_ENA
	HAL_Delay(1);

	/* Step 3: Disable 2.8V rail */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);   // IMG_ENA_A
	HAL_Delay(1);

	/* Step 4: Stop EXTCLK last */
	HAL_TIM_PWM_Stop(&htim11, TIM_CHANNEL_1);
	HAL_Delay(100);  // ensure caps discharge before next power up

	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);  // IMG_ENA
	HAL_Delay(1);

	Log("Camera A deactivated\r\n");
}

void DeactivateCAMB(void)
{
	/* Step 1: Assert reset first */
	CAM_ResetAssert();
	HAL_Delay(1);

	/* Step 2: Disable I2C level shifter */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET);  // IMG_I2C_ENA
	HAL_Delay(1);

	/* Step 3: Disable 2.8V rail */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);   // IMG_ENA_B
	HAL_Delay(1);

	/* Step 4: Stop EXTCLK last */
	HAL_TIM_PWM_Stop(&htim11, TIM_CHANNEL_1);
	HAL_Delay(100);  // ensure caps discharge before next power up

	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);  // IMG_ENA
	HAL_Delay(1);

	Log("Camera B deactivated\r\n");
}

HAL_StatusTypeDef CAM_WaitDoorbell(uint8_t i2c_addr)
{
    uint16_t cmd_reg = 0;
    uint32_t timeout = HAL_GetTick() + 200;  // 200ms timeout

    // Wait for doorbell bit 15 to clear
    do {
        if(HAL_GetTick() > timeout)
        {
            Log("CAM_WaitDoorbell TIMEOUT\r\n");
            return HAL_TIMEOUT;
        }
        CAM_ReadReg(i2c_addr, 0x0040, &cmd_reg);
    } while(cmd_reg & 0x8000);

    // Log result for debugging
    char log_buf[64];
    sprintf(log_buf, "Doorbell cleared, cmd_reg: 0x%04X\r\n", cmd_reg);
    Log(log_buf);

    return HAL_OK;
}

uint16_t CAM_GetState(uint8_t i2c_addr)
{
    uint16_t state = 0;

    // Issue Get State command — no parameters needed
    CAM_WriteReg(i2c_addr, 0x0040, 0x8101);

    // Wait for firmware to complete command
    if(CAM_WaitDoorbell(i2c_addr) != HAL_OK)
        return 0xFFFF;  // sentinel value — doorbell timed out

    // Read result from parameter pool
    CAM_ReadReg(i2c_addr, 0xFC00, &state);

    return state;
}

HAL_StatusTypeDef CAM_WriteReg(uint8_t i2c_addr, uint16_t reg, uint16_t val)
{
    uint8_t buf[2];
    buf[0] = (val >> 8) & 0xFF;   // data MSB first
    buf[1] = val & 0xFF;          // data LSB

    return HAL_I2C_Mem_Write(&hi2c2,
                              i2c_addr,
                              reg,
                              I2C_MEMADD_SIZE_16BIT,
                              buf,
                              2,
                              CAM_I2C_TIMEOUT);
}

HAL_StatusTypeDef CAM_ReadReg(uint8_t i2c_addr, uint16_t reg, uint16_t *val)
{
    uint8_t buf[2] = {0, 0};
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(&hi2c2,
                                              i2c_addr,
                                              reg,
                                              I2C_MEMADD_SIZE_16BIT,
                                              buf,
                                              2,
                                              CAM_I2C_TIMEOUT);
    *val = ((uint16_t)buf[0] << 8) | buf[1];
    return ret;
}

HAL_StatusTypeDef CAM_WriteReg32(uint8_t i2c_addr, uint16_t reg, uint32_t val)
{
    uint8_t buf[4];
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8)  & 0xFF;
    buf[3] =  val        & 0xFF;

    return HAL_I2C_Mem_Write(&hi2c2,
                              i2c_addr,
                              reg,
                              I2C_MEMADD_SIZE_16BIT,
                              buf,
                              4,
                              CAM_I2C_TIMEOUT);
}

HAL_StatusTypeDef CAM_ReadReg32(uint8_t i2c_addr, uint16_t reg, uint32_t *val)
{
    uint8_t buf[4] = {0};
    HAL_StatusTypeDef ret = HAL_I2C_Mem_Read(&hi2c2,
                                              i2c_addr,
                                              reg,
                                              I2C_MEMADD_SIZE_16BIT,
                                              buf,
                                              4,
                                              CAM_I2C_TIMEOUT);
    *val = ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  |
            (uint32_t)buf[3];
    return ret;
}

/* First version of CAM Init. Works for greyscale, but not for color
HAL_StatusTypeDef CAM_Init(uint8_t i2c_addr)
{
    HAL_StatusTypeDef ret;
    uint16_t state = 0;
    uint16_t readback = 0;
    uint32_t readback32 = 0;
    char log_buf[164];

    // Stage 1 — PAL progressive preset + Change-Config
    CAM_WriteReg(i2c_addr, 0x9826, 0x0025);			// PAL progressive preset
    CAM_WriteReg(i2c_addr, 0xFC00, 0x2800);			// Change-config
    CAM_WriteReg(i2c_addr, 0x0040, 0x8100);
    ret = CAM_WaitDoorbell(i2c_addr);
    if(ret != HAL_OK) return ret;

    state = CAM_GetState(i2c_addr);
    sprintf(log_buf, "State after stage 1: 0x%04X\r\n", state);
    Log(log_buf);

    // Stage 2 — PAL progressive scan mode + Change-Config
    // bit[2:1] = 01 PAL, bit[0] = 1 progressive
    CAM_WriteReg(i2c_addr, 0xC858, 0x0003);			// PAL progressive
    CAM_WriteReg(i2c_addr, 0xFC00, 0x2800);			// Change-config
    CAM_WriteReg(i2c_addr, 0x0040, 0x8100);
    ret = CAM_WaitDoorbell(i2c_addr);
    if(ret != HAL_OK) return ret;

    CAM_ReadReg(i2c_addr, 0xC858, &readback);
    sprintf(log_buf, "0xC858 after CC: 0x%04X\r\n", readback);
    Log(log_buf);

    // Stage 3 — parallel port config + Change-Config
    CAM_WriteReg32(i2c_addr, 0xC96C, 0x00000000);	// 0x00000000 for YCbCr, 0x00000100 for RGB565,  0x00000E00 for 8+2 bayer
    CAM_WriteReg(i2c_addr, 0xC972, 0x0005);         // Progressive, continuous PIXCLK, port enabled
    CAM_WriteReg(i2c_addr, 0x001E, 0x0601);		    // PIXCLK and DOUT slew rate

    // Disable AWB
    CAM_WriteReg(i2c_addr, 0xAC02, 0x0000);

    CAM_WriteReg(i2c_addr, 0xFC00, 0x2800);			// Change-config
    CAM_WriteReg(i2c_addr, 0x0040, 0x8100);
    ret = CAM_WaitDoorbell(i2c_addr);
    if(ret != HAL_OK) return ret;

    // Stage 4 — fix FOV alignment
    CAM_WriteReg(i2c_addr, 0xC85E, 0x02D0);  		// 720 active pixels
    CAM_WriteReg(i2c_addr, 0xC860, 0x0000);  		// first pixel = 0
    CAM_WriteReg(i2c_addr, 0xFC00, 0x2800);	 		// Change-config
    CAM_WriteReg(i2c_addr, 0x0040, 0x8100);
    ret = CAM_WaitDoorbell(i2c_addr);
    if(ret != HAL_OK) return ret;

    // Disable AWB — use manual gains
    //CAM_WriteReg(i2c_addr, 0xAC02, 0x0000);  // disable AWB

    // Final readbacks
    uint16_t r_gain, b_gain;
    CAM_ReadReg(i2c_addr, 0xAC12, &r_gain);
    CAM_ReadReg(i2c_addr, 0xAC14, &b_gain);
    sprintf(log_buf, "AWB R=0x%04X B=0x%04X\r\n", r_gain, b_gain);
    Log(log_buf);

	CAM_ReadReg(i2c_addr, 0xC858, &readback);
	sprintf(log_buf, "0xC858 final: 0x%04X\r\n", readback);
	Log(log_buf);

	CAM_ReadReg32(i2c_addr, 0xC96C, &readback32);
	sprintf(log_buf, "0xC96C final: 0x%08lX\r\n", readback32);
	Log(log_buf);

	CAM_ReadReg(i2c_addr, 0xC972, &readback);
	sprintf(log_buf, "0xC972 final: 0x%04X\r\n", readback);
	Log(log_buf);

	CAM_ReadReg(i2c_addr, 0x9826, &readback);
	sprintf(log_buf, "0x9826 final: 0x%04X\r\n", readback);
	Log(log_buf);

	uint16_t slew = 0;
	CAM_ReadReg(i2c_addr, 0x001E, &slew);
	sprintf(log_buf, "R0x001E slew final: 0x%04X\r\n", slew);
	Log(log_buf);

	uint16_t fov_active = 0;
	uint16_t fov_first  = 0;
	CAM_ReadReg(i2c_addr, 0xC85E, &fov_active);
	CAM_ReadReg(i2c_addr, 0xC860, &fov_first);
	sprintf(log_buf, "FOV active pixels: %d\r\n", fov_active);
	Log(log_buf);
	sprintf(log_buf, "FOV first pixel: %d\r\n", fov_first);
	Log(log_buf);

	state = CAM_GetState(i2c_addr);
	sprintf(log_buf, "State final: 0x%04X\r\n", state);
	Log(log_buf);

    return HAL_OK;
}*/

HAL_StatusTypeDef Photo_CaptureRaw(uint8_t  slot,
                                   uint16_t designator,
                                   uint8_t  *opcode)
{
	char log_buf[64];
    if (slot >= RAW_PHOTO_COUNT) return HAL_ERROR;

    volatile raw_photo_t *buf = RAW_BUFFER(slot);

    /* Fill header */
    buf->designator    = designator;
    buf->timestamp_MSB = (uint16_t)(HAL_GetTick() >> 16);
    buf->timestamp_LSB = (uint16_t)(HAL_GetTick() & 0xFFFF);
    buf->_pad          = 0x00;
    memcpy((void *)buf->opcode, opcode, OPCODE_SIZE);

    /* Arm DCMI */
    dcmi_frame_ready = 0;
    dcmi_error = 0;

    // Pre-configure DMA first
    HAL_DMA_Start_IT(hdcmi.DMA_Handle,
                     (uint32_t)&DCMI->DR,
                     (uint32_t)&buf->data[0],
                     H*L / 2);

    // Make sure interrupts are enabled
    __HAL_DCMI_ENABLE_IT(&hdcmi, DCMI_IT_FRAME);

    // Reset DCMI state for repeated captures
    HAL_DCMI_Stop(&hdcmi);
    HAL_Delay(1);

    // Reinitialize DCMI handle state
    hdcmi.State = HAL_DCMI_STATE_READY;

    HAL_StatusTypeDef ret = HAL_DCMI_Start_DMA(&hdcmi,
                                                DCMI_MODE_SNAPSHOT,
                                                (uint32_t)&buf->data[0],
                                                H * L / 2);


    sprintf(log_buf, "DCMI Start ret: %d\r\n", ret);
    Log(log_buf);

    // Read SR immediately
    sprintf(log_buf, "DCMI SR immediate: 0x%08lX\r\n", DCMI->SR);
    Log(log_buf);

    // Read CR register
    sprintf(log_buf, "DCMI CR: 0x%08lX\r\n", DCMI->CR);
    Log(log_buf);

	sprintf(log_buf, "DMA NDTR:  0x%08lX\r\n", hdcmi.DMA_Handle->Instance->NDTR);
	Log(log_buf);

    if (ret != HAL_OK) {
        Log("DCMI: start failed\r\n");
        return HAL_ERROR;
    }

    /* Wait for frame */
    uint32_t t0 = HAL_GetTick();
    while (!dcmi_frame_ready && !dcmi_error) {
        HAL_IWDG_Refresh(&hiwdg);
        if ((HAL_GetTick() - t0) > DCMI_TIMEOUT)
        {
        	sprintf(log_buf, "DCMI SR:   0x%08lX\r\n", DCMI->SR);
        	Log(log_buf);
        	sprintf(log_buf, "DCMI RISR: 0x%08lX\r\n", DCMI->RISR);
        	Log(log_buf);
        	sprintf(log_buf, "DCMI MISR: 0x%08lX\r\n", DCMI->MISR);
        	Log(log_buf);
        	sprintf(log_buf, "DCMI IER:  0x%08lX\r\n", DCMI->IER);
        	Log(log_buf);
        	sprintf(log_buf, "DMA NDTR:  0x%08lX\r\n", hdcmi.DMA_Handle->Instance->NDTR);
        	Log(log_buf);
        	sprintf(log_buf, "DMA CR:    0x%08lX\r\n", hdcmi.DMA_Handle->Instance->CR);
        	Log(log_buf);
            Log("DCMI: timeout\r\n");
            HAL_DCMI_Stop(&hdcmi);
            return HAL_TIMEOUT;
        }
    }

    if(dcmi_frame_ready)
    {
        uint32_t ndtr = hdcmi.DMA_Handle->Instance->NDTR;
        uint32_t transferred = (H * L / 2) - ndtr;
        sprintf(log_buf, "DMA NDTR after capture: 0x%08lX\r\n", ndtr);
        Log(log_buf);
        sprintf(log_buf, "Words transferred: %lu\r\n", transferred);
        Log(log_buf);
        sprintf(log_buf, "Bytes transferred: %lu\r\n", transferred * 4);
        Log(log_buf);
        sprintf(log_buf, "Pixels transferred: %lu\r\n", transferred * 2);
        Log(log_buf);
    }


    Log("DCMI: frame captured\r\n");
    return HAL_OK;
}

