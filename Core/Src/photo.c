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
    /* Step 1: Assert reset */
    CAM_ResetAssert();
    HAL_Delay(1);

    /* Step 1b: Start 27 MHz EXTCLK — must exist before rails come up */
    if (HAL_TIM_PWM_Start(&htim11, TIM_CHANNEL_1) != HAL_OK) {
        Log("EXTCLK start FAILED\r\n");
        Error_Handler();
    }
    HAL_Delay(1);   // let clock stabilise

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);  // IMG_ENA
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
    HAL_Delay(150);  // increased from 100ms to cover regulator settle + boot

    Log("Camera A activated\r\n");
}

/* ------------------------------------------------------------------ */
/*  Camera B full init sequence                                        */
/* ------------------------------------------------------------------ */
void ActivateCAMB(void)
{
    /* Step 1: Assert reset */
    CAM_ResetAssert();
    HAL_Delay(1);

    /* Step 1b: Start 27 MHz EXTCLK — must exist before rails come up */
    if (HAL_TIM_PWM_Start(&htim11, TIM_CHANNEL_1) != HAL_OK) {
        Log("EXTCLK start FAILED\r\n");
        Error_Handler();
    }
    HAL_Delay(1);   // let clock stabilise

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_SET);  // IMG_ENA
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
}

void DeactivateCAMA(void)
{
    /* Step 1: Assert reset first
     * This stops the sensor driving VREG_BASE,
     * so the BSR16 stops conducting and 1.8V begins to collapse */
    CAM_ResetAssert();
    HAL_Delay(5);    // allow 1.8V to start collapsing before killing 2.8V

    /* Step 1b: Stop EXTCLK — no point running with sensor in reset */
    HAL_TIM_PWM_Stop(&htim11, TIM_CHANNEL_1);

    /* Step 2: Disable I2C level shifter
     * Isolate the bus before 2.8V rail dies to prevent
     * spurious I2C transactions from floating lines */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET); // IMG_I2C_ENA
    HAL_Delay(1);

    /* Step 3: Kill 2.8V rail
     * 1.8V is already collapsing from Step 1 so no reverse
     * bias risk on internal ESD structures */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, GPIO_PIN_RESET);  // IMG_ENA_A
    HAL_Delay(1);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET); // IMG_ENA

    /* Step 4: Wait for all caps to fully discharge
     * 4.7uF on 1.8V + bulk caps on 2.8V need time to drain
     * before any re-enable attempt */
    HAL_Delay(100);

    Log("Camera A deactivated\r\n");
}

void DeactivateCAMB(void)
{
    /* Same sequence, different rail pin */
    CAM_ResetAssert();
    HAL_Delay(5);

    /* Step 1b: Stop EXTCLK — no point running with sensor in reset */
    HAL_TIM_PWM_Stop(&htim11, TIM_CHANNEL_1);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_RESET); // IMG_I2C_ENA
    HAL_Delay(1);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);  // IMG_ENA_B

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET); // IMG_ENA

    HAL_Delay(100);

    Log("Camera B deactivated\r\n");
}


HAL_StatusTypeDef CAM_WriteReg(uint8_t i2c_addr, uint16_t reg, uint16_t val)
{
    uint8_t buf[2];
    buf[0] = (val >> 8) & 0xFF;
    buf[1] =  val       & 0xFF;

    return HAL_I2C_Mem_Write(&hi2c2,
                              i2c_addr,
                              reg,
                              I2C_MEMADD_SIZE_16BIT,
                              buf,
                              2,
                              I2C_TIMEOUT);
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
											  I2C_TIMEOUT);

    *val = ((uint16_t)buf[0] << 8) | buf[1];
    return ret;
}

HAL_StatusTypeDef Camera_CommsTest(uint8_t i2c_addr)
{
    uint16_t val = 0;
    char log_buf[48];

    if (CAM_ReadReg(i2c_addr, 0x0000, &val) != HAL_OK) {
        Log("CAM: I2C transaction failed\r\n");
        return HAL_ERROR;
    }

    if (val != 0x2285) {
        snprintf(log_buf, sizeof(log_buf),
                 "CAM: wrong device ID 0x%04X\r\n", val);
        Log(log_buf);
        return HAL_ERROR;
    }

    if (CAM_ReadReg(i2c_addr, 0x001C, &val) != HAL_OK) {
        Log("CAM: firmware version read failed\r\n");
        return HAL_ERROR;
    }

    snprintf(log_buf, sizeof(log_buf),
             "CAM: ID OK, FW version 0x%04X\r\n", val);
    Log(log_buf);

    return HAL_OK;
}

// Function to write with i2c to CAM with verify to make sure register configurations are applied and not reset to default
static HAL_StatusTypeDef CAM_WriteVerify(uint8_t  i2c_addr,
                                          uint16_t reg,
                                          uint16_t val,
                                          uint8_t  retries)
{
    uint16_t readback = 0;
    char log_buf[148];

    for (uint8_t i = 0; i < retries; i++)
    {
        if (CAM_WriteReg(i2c_addr, reg, val) != HAL_OK) continue;
        HAL_Delay(5);
        if (CAM_ReadReg(i2c_addr, reg, &readback) != HAL_OK) continue;

        if (readback == val) return HAL_OK;

        snprintf(log_buf, sizeof(log_buf),
                 "CAM: 0x%04X wrote 0x%04X read 0x%04X retry %d\r\n",
                 reg, val, readback, i + 1);
        Log(log_buf);
        HAL_Delay(10);
    }

    snprintf(log_buf, sizeof(log_buf),
             "CAM: 0x%04X failed after %d retries\r\n", reg, retries);
    Log(log_buf);
    return HAL_ERROR;
}

HAL_StatusTypeDef ASX340AT_Init(uint8_t i2c_addr)
{
    uint16_t val     = 0;
    char     log_buf[48];
    uint32_t t0;

    /* ---- 1. Verify alive ------------------------------------- */
    if (CAM_ReadReg(i2c_addr, 0x0000, &val) != HAL_OK) {
        Log("CAM: I2C failed\r\n");
        return HAL_ERROR;
    }
    if (val != 0x2285) {
        Log("CAM: wrong device ID\r\n");
        return HAL_ERROR;
    }

    /* ---- 2. Configure parallel output ----------------------- */
    /* NTSC parallel enable — hardware register                  */
    CAM_WriteReg(i2c_addr, 0x9426, 0x0025);

    /* Progressive scan                                          */
    /* cam_frame_scan_control [0]=1                              */
    CAM_WriteReg(i2c_addr, 0xC858, 0x0001);

    /* Parallel port: progressive source, enabled               */
    /* cam_port_parallel_control [2:1]=10, [0]=1 = 0x0005       */
    CAM_WriteReg(i2c_addr, 0xC972, 0x0005);

    /* ---- 3. Change-Config — applies settings ----------------- */
    /* Sensor is in "Wait for Host Command" after Auto-Config.   */
    /* Change-Config applies our register writes then returns    */
    /* to "Wait for Host Command". Does NOT start streaming.     */
    CAM_WriteReg(i2c_addr, 0xFC00, 0x2800);
    CAM_WriteReg(i2c_addr, 0x0040, 0x8100);

    t0 = HAL_GetTick();
    do {
        HAL_Delay(10);
        if (CAM_ReadReg(i2c_addr, 0x0040, &val) != HAL_OK) {
            Log("CAM: CC poll failed\r\n");
            return HAL_ERROR;
        }
        if ((HAL_GetTick() - t0) > 1000) {
            Log("CAM: CC timeout\r\n");
            return HAL_TIMEOUT;
        }
    } while (val & 0x8000);

    if ((val & 0x7F00) != 0x0000) {
        Log("CAM: CC error\r\n");
        return HAL_ERROR;
    }
    Log("CAM: CC complete\r\n");

    HAL_Delay(50);

    /* ---- 4. Rewrite variables reset by Change-Config --------- */
    CAM_WriteReg(i2c_addr, 0xC972, 0x0005);
    CAM_WriteReg(i2c_addr, 0x3C00, 0x0001);
    CAM_WriteReg(i2c_addr, 0x3642, 0x0001);

    /* ---- 5. Set_State(0x31) — start streaming ---------------- */
    /* This transitions sensor from "Wait for Host Command"      */
    /* to active streaming state.                                */
    CAM_WriteReg(i2c_addr, 0xFC00, 0x3100);
    CAM_WriteReg(i2c_addr, 0x0040, 0x8100);

    t0 = HAL_GetTick();
    do {
        HAL_Delay(10);
        if (CAM_ReadReg(i2c_addr, 0x0040, &val) != HAL_OK) {
            Log("CAM: stream start poll failed\r\n");
            return HAL_ERROR;
        }
        if ((HAL_GetTick() - t0) > 1000) {
            Log("CAM: stream start timeout\r\n");
            return HAL_TIMEOUT;
        }
    } while (val & 0x8000);

    if ((val & 0x7F00) != 0x0000) {
        Log("CAM: stream start error\r\n");
        return HAL_ERROR;
    }
    Log("CAM: stream start complete\r\n");

    HAL_Delay(50);

    /* ---- 6. Verify state ------------------------------------ */
    uint16_t sysmgr  = 0;
    uint16_t r0018   = 0;
    uint16_t c858    = 0;
    uint16_t c972    = 0;
    uint16_t inttime1 = 0, inttime2 = 0;

    CAM_ReadReg(i2c_addr, 0xDC00, &sysmgr);
    CAM_ReadReg(i2c_addr, 0x0018, &r0018);
    CAM_ReadReg(i2c_addr, 0xC858, &c858);
    CAM_ReadReg(i2c_addr, 0xC972, &c972);

    snprintf(log_buf, sizeof(log_buf),
             "SYSMGR = 0x%04X\r\n", sysmgr);
    Log(log_buf);
    snprintf(log_buf, sizeof(log_buf),
             "0018=%04X C858=%04X C972=%04X\r\n",
             r0018, c858, c972);
    Log(log_buf);

    CAM_ReadReg(i2c_addr, 0xC840, &inttime1);
    HAL_Delay(100);
    CAM_ReadReg(i2c_addr, 0xC840, &inttime2);
    snprintf(log_buf, sizeof(log_buf),
             "inttime: 0x%04X → 0x%04X\r\n",
             inttime1, inttime2);
    Log(log_buf);

    /* ---- 7. Check DCMI SR ----------------------------------- */
    snprintf(log_buf, sizeof(log_buf),
             "DCMI SR = 0x%02lX\r\n",
             (uint32_t)(DCMI->SR & 0x07));
    Log(log_buf);

    Log("CAM: init complete, streaming started\r\n");
    return HAL_OK;
}

HAL_StatusTypeDef Photo_CaptureRaw(uint8_t  slot,
                                   uint16_t designator,
                                   uint8_t  *opcode)
{
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
    dcmi_error       = 0;

    HAL_StatusTypeDef ret = HAL_DCMI_Start_DMA(&hdcmi,
                                                DCMI_MODE_SNAPSHOT,
                                                (uint32_t)&buf->data[0],
                                                (L * H) / 2);
    if (ret != HAL_OK) {
        Log("DCMI: start failed\r\n");
        return HAL_ERROR;
    }

    /* Wait for frame */
    uint32_t t0 = HAL_GetTick();
    while (!dcmi_frame_ready && !dcmi_error) {
        HAL_IWDG_Refresh(&hiwdg);
        if ((HAL_GetTick() - t0) > 3000) {
            Log("DCMI: timeout\r\n");
            HAL_DCMI_Stop(&hdcmi);
            return HAL_TIMEOUT;
        }
    }

    if (dcmi_error) {
        Log("DCMI: error\r\n");
        return HAL_ERROR;
    }

    Log("DCMI: frame captured\r\n");
    return HAL_OK;
}

