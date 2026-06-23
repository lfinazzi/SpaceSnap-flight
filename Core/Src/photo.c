#include "photo.h"
#include "comms.h"
#include "main.h"

#define TJE_IMPLEMENTATION
#include <tiny_jpeg.h>

extern I2C_HandleTypeDef hi2c2;
extern TIM_HandleTypeDef htim11;
extern DCMI_HandleTypeDef hdcmi;
extern IWDG_HandleTypeDef hiwdg;

//DMA_HandleTypeDef hdma_dcmi;

volatile uint8_t dcmi_frame_ready 	= 0;   // set by callback, cleared by main
volatile uint8_t dcmi_error       	= 0;   // set on DCMI error

cam_params_t cam_params;
uint8_t current_metadata_slot;			// Variable that holds current free slot in SRAM for metadata of compressed images
uint32_t current_data_address_sram;		// Variable that holds the address in SRAM for next compressed write

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
	    return;
	}
	sprintf(log_buf, "Camera A chip_id: 0x%04X\r\n", chip_id);
	Log(log_buf);

	state = CAM_GetState(CAM_I2C_ADDR_A);
	if(state == 0xFFFF)
	{
	    Log("Camera A GetState FAILED\r\n");
	    return;
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
        return;
    }
    sprintf(log_buf, "Camera B chip_id: 0x%04X\r\n", chip_id);
    Log(log_buf);

    state = CAM_GetState(CAM_I2C_ADDR_B);
    if(state == 0xFFFF)
    {
        Log("Camera B GetState FAILED\r\n");
        return;
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

HAL_StatusTypeDef CAM_ChangeConfig(uint8_t i2c_addr)
{
    /* CMD_HANDLER_PARAMS_POOL_0: Set_State command opcode 0x28 */
    CAM_WriteReg(i2c_addr, 0xFC00, 0x2800);
    /* COMMAND_REGISTER: doorbell bit + host command flag */
    CAM_WriteReg(i2c_addr, 0x0040, 0x8100);
    /* Poll DOORBELL bit until SOC clears it (command accepted) */
    return CAM_WaitDoorbell(i2c_addr);
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


HAL_StatusTypeDef CAM_Init(uint8_t i2c_addr)
{
    HAL_StatusTypeDef ret;
    uint16_t readback = 0;
    char log_buf[128];

    /* =========================================================================
     * STAGE 0 — Power-on stabilization and device ID verification
     *
     * After RESET_BAR is deasserted the SOC runs its boot sequence:
     *   1. Flash Detection: probes SPI bus for external NVM
     *   2. SPI_SDI sampling: HIGH (internal pull-up) → Auto-Config mode
     *   3. Auto-Config: samples GPIO[8:11] for orientation/format, then issues
     *      Change-Config internally and starts streaming
     *
     * Auto-Config GPIO sampling is the first dangerous moment:
     *   DOUT_LSB0  (GPIO[10]): NO pull-up. Selects NTSC(0) or PAL(1).
     *   DOUT_LSB1  (GPIO[11]): NO pull-up. Selects vertical flip.
     *   FRAME_VALID(GPIO[9]) : NO pull-up. Selects horizontal mirror.
     *   LINE_VALID (GPIO[8]) : NO pull-up. Selects 7.5 IRE pedestal.
     * All four pins are floating through a 25cm cable. Sampled values are
     * indeterminate. Stage 1 explicitly corrects all affected registers.
     *
     * Minimum time from RESET_BAR deassertion to I2C-ready: ~200ms typical.
     * The sensor is already streaming by the time we begin writing.
     * ========================================================================= */

    HAL_Delay(200);

    /* Read device ID register (SYSCTL 0x0000, read-only).
     * ASX340AT always returns 0x2285. Any other value means either the sensor
     * is not yet ready, the I2C address is wrong, or there is a wiring fault. */
    CAM_ReadReg(i2c_addr, 0x0000, &readback);
    sprintf(log_buf, "Device ID: 0x%04X (expected 0x2285)\r\n", readback);
    Log(log_buf);
    if (readback != 0x2285) {
        Log("ERROR: Device ID mismatch. Check I2C address and wiring.\r\n");
        return HAL_ERROR;
    }


    /* =========================================================================
     * STAGE 1 — Scan mode, image orientation, flicker avoidance
     *
     * These three settings are written together before a single Change-Config
     * because they are all frame-start-synchronized and logically coupled
     * (PAL progressive ↔ 50Hz flicker avoidance).
     * ========================================================================= */

    /* PAL progressive preset for the analog composite DAC driver.
     * AND9270/D: "[PAL: Enable VGA Progressive on Parallel Port] REG = 0x9826, 0x25"
     *
     * Address 0x9826 = PAL firmware driver (driver 6), offset 0x26.
     * This is a DIFFERENT register from the NTSC equivalent at 0x9426.
     *
     * Written even when using digital parallel output only: the PAL driver
     * initializes shared SOC clock structures used by both analog and digital
     * output paths. Without this preset the SOC may default to NTSC composite
     * encoder timing, which corrupts the YCbCr pixel clock relationship for
     * progressive scan on the parallel port. */
    CAM_WriteReg(i2c_addr, 0x9826, 0x0025);

    /* cam_frame_scan_control (CamControl driver 18, direct address 0xC858)
     *
     * Bit layout:
     *   bits[4:3] — progressive rate: 00=VGA60(60fps), 01=VGA50(50fps), 10=Custom
     *   bits[2:1] — interlaced format: 00=NTSC, 01=PAL  [irrelevant when bit[0]=1]
     *   bit[0]    — scan type: 0=interlaced, 1=progressive
     *
     * Value 0x0009 = 0b0000_1001:
     *   bits[4:3] = 01 → VGA50 (50fps, matches EU PAL mains at 50Hz)
     *   bit[0]    =  1 → progressive scan
     *
     */
    CAM_WriteReg(i2c_addr, 0xC858, 0x0009);

    /* cam_sensor_control_read_mode (CamControl 0xC838)
     * Controls image flip and mirror.
     *
     * bits[1:0]:
     *   0x0 = Normal (no transformation)
     *   0x1 = Horizontal mirror
     *   0x2 = Vertical flip
     *   0x3 = Rotate 180° (flip + mirror)
     *
     * CRITICAL FIX for floating GPIO pins:
     * Auto-Config samples GPIO[9] (FRAME_VALID) and GPIO[11] (DOUT_LSB1) during
     * boot to set horizontal mirror and vertical flip respectively.
     * Both pins are floating through the 25cm cable. The STM32 DCMI is not yet
     * configured so both ends of the cable are floating simultaneously.
     * The sampled voltage is determined by board-level parasitics and power supply
     * bounce — it is completely unpredictable on each power cycle.
     * This explicit write overrides whatever Auto-Config set.
     *
     * Adjust to 0x0002 (vertical flip) or 0x0003 (rotate 180°) if your
     * physical lens/PCB mounting requires a different orientation.
     * Needs Change-Config to take effect. */
    CAM_WriteReg(i2c_addr, 0xC838, 0x0000);			// TODO: Change on definitive board camera position to avoid rotation

    /* CAM_AET_FLICKER_FREQ_HZ (CamControl 0xC881)
     * Sets the reference frequency for flicker avoidance.
     *
     * The AE algorithm constrains integration time to integer multiples of the
     * light source flicker period. AC-powered lighting (fluorescent, LED drivers)
     * flickers at 2× mains frequency: 100Hz in EU (50Hz mains), 120Hz in US.
     * If integration time is not a multiple of the flicker period, alternating
     * light/dark horizontal bands appear in the captured image.
     *
     * 0x0032 = 50 decimal → 50Hz flicker avoidance (EU/PAL, 100Hz lamp flicker)
     * 0x003C = 60 decimal → 60Hz flicker avoidance (US/NTSC, 120Hz lamp flicker)
     *
     * This value must match the mains frequency at the deployment location Currently 50 Hz (ARG). */
    CAM_WriteReg(i2c_addr, 0xC881, 0x0032);

    /* Apply scan mode, orientation, and flicker settings. */
    ret = CAM_ChangeConfig(i2c_addr);
    if (ret != HAL_OK) {
        Log("ERROR: Stage 1 Change-Config failed\r\n");
        return ret;
    }

    /* Verify scan mode was accepted */
    CAM_ReadReg(i2c_addr, 0xC858, &readback);
    sprintf(log_buf, "0xC858 scan mode: 0x%04X\r\n", readback);
    Log(log_buf);


    /* =========================================================================
     * STAGE 2 — Output format and parallel port
     *
     * Configures the 8-bit digital parallel interface connected to STM32 DCMI.
     * These settings define what the STM32 receives on DOUT[7:0].
     * ========================================================================= */

    /* cam_output_format (CamControl 0xC96C)
     * Selects the pixel data format clocked out on DOUT[7:0].
     *
     * Bit layout:
     *   bits[13:12] — RGB sub-format    (only active when bits[9:8]=01)
     *   bits[11:10] — Bayer sub-format  (only active when bits[9:8]=10)
     *   bits[9:8]   — Primary format:   00=YCbCr, 01=RGB, 10=Bayer, 11=None
     *   bit[2]      — Monochrome enable: 0=color, 1=Y-only (Cb/Cr forced to 128)
     *   bit[1]      — Byte swap:         0=UYVY, 1=YUYV
     *   bit[0]      — Channel swap:      0=normal Cb/Cr, 1=swap Cb/Cr
     *
     * Value 0x0000:
     *   bits[9:8] = 00 → YCbCr 4:2:2 output
     *   bit[1]    =  0 → UYVY byte order: Cb₀ Y₀ Cr₀ Y₁ Cb₂ Y₂ ...
     *   bit[0]    =  0 → standard Cb/Cr ordering
     *
     * Memory layout after DCMI DMA (little-endian, 32-bit words):
     *   base+0: Cb₀  base+1: Y₀  base+2: Cr₀  base+3: Y₁   ← pixel pair 0
     *   base+4: Cb₂  base+5: Y₂  base+6: Cr₂  base+7: Y₃   ← pixel pair 1
     *
     * This is standard UYVY, directly compatible with:
     *   OpenCV: CV_YUV2BGR_UYVY
     *   libyuv: UYVYToI420 / UYVYToARGB
     *
     * Change to 0x0002 if your processing pipeline expects YUYV instead of UYVY.
     * Do NOT use 0xC96C for Bayer output (bits[9:8]=10) unless DOUT_LSB[1:0] are
     * physically wired to the STM32 — the two LSBs would be lost silently.
     */

    CAM_WriteReg(i2c_addr, 0xC96C, 0x0000);

    /* cam_port_parallel_control (CamControl 0xC972)
     * Configures the parallel output port operation.
     *
     * Bit layout:
     *   bit[4]   — PIXCLK gating: 0=continuous, 1=gate PIXCLK during blanking
     *   bits[2:1]— output source: 00=reserved, 01=interlaced, 10=progressive, 11=reserved
     *   bit[0]   — port enable:   0=disabled (all pins driven to logic 0), 1=enabled
     *
     * Value 0x0005 = 0b0000_0101:
     *   bit[4]   = 0 → PIXCLK continuous (REQUIRED for STM32 DCMI hardware sync mode)
     *   bits[2:1]= 10 → progressive source (matches scan mode in 0xC858)
     *   bit[0]   = 1  → port enabled
     *
     * Continuous PIXCLK is mandatory: the STM32 DCMI peripheral in hardware
     * synchronization mode counts PIXCLK edges to synchronize DMA transfers.
     * If PIXCLK stops during blanking (gate=1), the DCMI loses count and
     * subsequent frames are misaligned in memory. */

    CAM_WriteReg(i2c_addr, 0xC972, 0x0005);

    /* pad_slew (SYSCTL 0x001E) — Output pin slew rate control
     *
     * Bit layout:
     *   bits[10:8] — slew_pixclk: PIXCLK slew rate code
     *   bits[6:4]  — slew_spi:    SPI bus slew rate (SPI not used at runtime)
     *   bits[2:0]  — slew_dout:   DOUT[7:0], FRAME_VALID, LINE_VALID, GPIO12/13
     *
     * IMPORTANT: slew_pixclk codes 0 and 1 produce NO CLOCK OUTPUT on PIXCLK.
     * Minimum functional code is 2 (7ns rise time).
     *
     * Rise time table at 40pF load (AND9270/D Table 13):
     *   Code  PIXCLK rise    DOUT rise
     *     0   ---- (N/A) ----         PIXCLK non-functional at codes 0 and 1
     *     1   ---- (N/A) ----
     *     2   7.0 ns         6.8 ns
     *     3   5.2 ns         5.2 ns
     *     4   4.0 ns         3.8 ns   ← DOUT recommended with 47Ω series R
     *     5   3.0 ns         3.3 ns
     *     6   2.4 ns         3.0 ns   ← PIXCLK recommended with 47Ω series R
     *     7   1.9 ns         2.8 ns
     *
     * SIGNAL PATH ANALYSIS (sensor → 15cm cable → 10cm PCB → STM32 DCMI):
     *
     *   PIXCLK = 54 MHz → period = 18.5 ns → half-period = 9.3 ns
     *   Total path length = 25 cm
     *   Propagation delay = 25 cm ÷ 15 cm/ns ≈ 1.67 ns one-way
     *   Estimated load capacitance ≈ 34 pF (cable + PCB + GPIO input)
     *
     *   TRANSMISSION LINE CRITERION: T_rise < 2×T_flight = 3.33 ns
     *   All codes 2–7 exceed this threshold → all produce reflections.
     *   Unterminated reflection at STM32 input: voltage doubles on arrival,
     *   producing an overshoot that exceeds VDD_IO + 0.3V absolute maximum.
     *
     *   HARDWARE FIX REQUIRED:
     *   47–56 Ω series resistors at the sensor output on PIXCLK + DOUT[7:0].
     *   Source termination absorbs the reflected wave at the driver,
     *   producing a clean single-step transition at the STM32 end.
     *   Without series resistors, GPIO damage accumulates over time.
     *
     * Value 0x0604 with series resistors fitted:
     *   bits[10:8] = 6 → slew_pixclk = 2.4 ns rise  (fast, SI handled by R_s)
     *   bits[6:4]  = 0 → slew_spi    = slowest       (SPI unused at runtime)
     *   bits[2:0]  = 4 → slew_dout   = 3.8 ns rise
     *
     * Alternative 0x0303 WITHOUT series resistors (fallback, less SI):
     *   bits[10:8] = 3 → slew_pixclk = 5.2 ns rise   (slower reduces overshoot)
     *   bits[2:0]  = 3 → slew_dout   = 5.2 ns rise
     *   Note: even at code 3, T_rise/6 = 0.87 ns < T_flight 1.67 ns,
     *   so reflections still occur at reduced amplitude. Not a complete fix.
     *
     * Currently using PIXCLK much slower: 13.5 MHz
     * pad_slew takes effect immediately (hardware register, no Change-Config). */

    CAM_WriteReg(i2c_addr, 0x001E, 0x0200);

    /* FOV alignment
     *
     * These two registers select the PAL stretch and pixel start for this format. Output for PAL is 720 x 576
     *
     */
    CAM_WriteReg(i2c_addr, 0xC85E, 0x02D0);  		// 720 active pixels, must be even between 692 and 720
    CAM_WriteReg(i2c_addr, 0xC860, 0x0000);			// PIxel offset 0, must be less or equal to 14

    /* Apply output format and parallel port configuration. */
    ret = CAM_ChangeConfig(i2c_addr);
    if (ret != HAL_OK) {
        Log("ERROR: Stage 2 Change-Config failed\r\n");
        return ret;
    }

    CAM_ReadReg(i2c_addr, 0xC96C, &readback);
    sprintf(log_buf, "0xC96C output format: 0x%04X\r\n", readback);
    Log(log_buf);
    CAM_ReadReg(i2c_addr, 0xC972, &readback);
    sprintf(log_buf, "0xC972 parallel ctrl: 0x%04X\r\n", readback);
    Log(log_buf);
    CAM_ReadReg(i2c_addr, 0x001E, &readback);
    sprintf(log_buf, "0x001E pad slew:      0x%04X\r\n", readback);
    Log(log_buf);


    /* =========================================================================
     * STAGE 3 — Auto Exposure configuration
     *
     * AE setup follows the pattern from MT9V125 REV4 Demo Init (Rev4-3NoRowNC):
     *   1. Select AE algorithm
     *   2. Set AE mode (flicker avoidance policy)
     *   3. Reset AE state: force target to zero, wait, then set real target
     *
     * The forced-zero → delay → real-target pattern ensures the AE algorithm
     * starts convergence from a known clean state rather than continuing from
     * the Auto-Config operation's accumulated state.
     * ========================================================================= */

    /* ae_rule_algo (AE_Rule driver 9, direct address 0xA404)
     * Selects the AE metering algorithm used to calculate scene brightness.
     *
     * 0 = Average Brightness Tracking (ABT):
     *     Simple uniform average of all 25 zones. Appropriate for scenes
     *     with uniform lighting and no strong foreground/background contrast.
     *
     * 1 = Weighted Average Brightness:
     *     Each of the 25 zones has a configurable weight (0xA407–0xA41F).
     *     Default weights are center-heavy. Good for general camera use.
     *
     * 2 = Adaptive for highlights:
     *     Exposes based on brighter zones. Prevents highlight clipping.
     *     Foreground subject underexposed when background is bright.
     *
     * 3 = Adaptive for lowlights:
     *     Exposes based on darker zones. Correctly exposes dark foreground
     *     subjects even when background is bright. Subject tracked even
     *     when off-center — important for rear-view camera applications
     *     where the vehicle or obstacle may not be centered in frame.
     *.
     */

    //CAM_WriteReg(i2c_addr, 0xA404, ae_rule_algo_val);

    /* cam_aet_aemode (CamControl 0xC86C)
     * Enables auxiliary AE modes. Three independent bits, combinable.
     *
     * bit[0] — Indoor AE (flicker-locking):
     *   When set, integration time is always an integer multiple of the flicker
     *   period (set by CAM_AET_FLICKER_FREQ_HZ at 0xC881). This guarantees
     *   zero flicker banding at the cost of slightly reduced dynamic range under
     *   extremely bright conditions (minimum integration = 1 flicker period).
     *
     * bit[1] — Discrete Frame Rate (VGA50/60 progressive only):
     *   Frame rate varies in powers of 2 (50→25→12.5 fps) as scene darkens.
     *   Only valid in progressive custom scan mode. Not applicable here (VGA50).
     *
     * bit[2] — Adaptive AE Target:
     *   Different luma targets for bright vs dark scenes. Requires additional
     *   configuration of cam_ll_start_target_luma_bm and cam_ll_stop_target_luma_bm.
     *
     * Value 0x0001: Indoor AE only. Flicker avoidance is the highest priority
     * in automotive applications with varied indoor/outdoor environments. */

    //CAM_WriteReg(i2c_addr, 0xC86C, 0x0001);

    /* AE state reset — Step 1: Force target luma to zero.
     * cam_aet_target_average_luma (CamControl 0xC86E) bits[7:0]
     *
     * Setting target = 0 causes the AE algorithm to immediately drive the sensor
     * to minimum exposure (shortest integration time, lowest gain) on the next
     * frame. All accumulated exposure state from Auto-Config operation is flushed.
     * The sensor has been streaming since Auto-Config completed, potentially with
     * incorrect AE state. This ensures we start from a clean baseline. */

    //CAM_WriteReg(i2c_addr, 0xC86E, 0x0000);

    /* Apply AE mode and force zero AE target. */

    //ret = CAM_ChangeConfig(i2c_addr);
    //if (ret != HAL_OK) {
        //Log("ERROR: Stage 3a Change-Config failed\r\n");
        //return ret;
    //}

    /* Wait for AE to reach the zero exposure state.
     * AE runs one adjustment step per frame. At 50fps: 250ms ≈ 12 frames.
     * This is sufficient to drive exposure to minimum before we set the real target.
     * Pattern derived directly from MT9V125 REV4 Demo Init (Rev4-3NoRowNC). */
    HAL_Delay(250);

    /* AE state reset — Step 2: Set the operational AE target.
     * cam_aet_target_average_luma (0xC86E) = 0x3C = 60
     *
     * AE converges until the scene average luminance equals 60 out of 255
     * (~24% of full scale). This is intentionally conservative:
     * — Highlights (sky, headlights, reflections) are not clipped
     * — Detail in dark regions is preserved for dynamic scenes
     * — Value derived from MT9V125 Rev4 Demo Init (AE_PRECISION_TARGET 0x083C,
     *   lower byte = target luma = 0x3C = 60)
     *
     * Increase toward 80–100 if you need a brighter average exposure.
     * Decrease toward 40 if highlight preservation is critical. */

    //CAM_WriteReg(i2c_addr, 0xC86E, 0x003C);

    /* ae_track_gate_percentage (AE_Track driver 10, direct address 0xA808)
     * Hysteresis gate as a percentage of the AE target brightness.
     * AE stops making adjustments when current_luma is within target ± (gate% × target).
     *
     *   Value 0x000D = 13%:
     *   Gate window = ±13% × 60 ≈ ±7.8 luma counts around target.
     *   AE settles when scene luma is in range [52, 68].
     *   Equivalent to the ±8 count stability range in MT9V125 AE_PRECISION_TARGET.
     *   Prevents micro-hunting: continuous small gain adjustments that cause
     *   perceptible brightness flicker in steady illumination. */

    //CAM_WriteReg(i2c_addr, 0xA808, 0x000D);

    /* Apply AE target and gate. */
    //ret = CAM_ChangeConfig(i2c_addr);
    //if (ret != HAL_OK) {
        //Log("ERROR: Stage 3b Change-Config failed\r\n");
        //return ret;
    //}


    /* =========================================================================
     * STAGE 4 — AWB state reset via Manual WB toggle
     *
     * Derived from MT9V125 REV4 Demo Init (appears in BOTH Rev4-3NoRowNC and
     * CCM_Improved_Facial_Color sections — it is applied twice in the .ini).
     *
     * This is NOT about using manual white balance. It is a state flush.
     *
     * The AWB driver maintains internal state across frames:
     *   — Gain accumulators: running sums of R/G and B/G ratios
     *   — Convergence counters: history of how many frames AWB has been stable
     *   — CCM interpolation position: current matrix blend position (0–127)
     *   — Statistical history: recent zone measurements
     *
     * After Stage 4 wrote new gain ratios, this state still reflects
     * the Auto-Config operation (which ran with power-on default parameters).
     * AWB may converge to a wrong CCM position because its accumulators
     * contain measurements that do not correspond to the new gain ratio anchors.
     *
     * Toggling cam_awb_mode_auto (bit[1] of 0xC8FF) to manual:
     *   — Freezes AWB gain output (gains held at current values)
     *   — Clears all internal AWB accumulators and convergence counters
     *   — Resets CCM interpolation position to the center (middle matrix)
     * The 500ms hold ensures the flush completes fully (25 frames at 50fps).
     * Returning to auto mode restarts AWB from a clean, centered position
     * using the new gain ratio anchors as reference points.
     *
     * Without this reset, AWB can take 10–30 additional seconds to converge
     * correctly, or lock onto a wrong color temperature entirely.
     *
     * cam_awb_awbmode register (CamControl 0xC8FF):
     *   bit[1] = cam_awb_mode_auto: 1=auto AWB, 0=manual (gains frozen)
     *   bit[0] = always written 1
     *   0x0001 = manual mode (AWB frozen, state flush)
     *   0x0003 = auto mode  (AWB active)
     * ========================================================================= */

    //CAM_WriteReg(i2c_addr, 0xC8FF, 0x0001);    /* Manual WB ON: freeze AWB, flush state */
    //HAL_Delay(500);                             /* Hold 500ms: 25 frames at 50fps        */
    //CAM_WriteReg(i2c_addr, 0xC8FF, 0x0003);    /* Auto WB ON:  restart AWB from clean state */

    /* Apply the AWB mode transition. */
    //ret = CAM_ChangeConfig(i2c_addr);
    //if (ret != HAL_OK) {
        //Log("ERROR: Stage 5 Change-Config failed\r\n");
        //return ret;
    //}


    /* =========================================================================
     * STAGE 6 — Final register readback verification
     *
     * Read back every register written in the init sequence.
     * Any value differing from expected indicates an I2C write failure,
     * register address error, or a register that requires a different
     * Change-Config sequence to take effect.
     * ========================================================================= */

    Log("=== CAM_Init final verification ===\r\n");

    struct { uint16_t addr; uint16_t expected; const char *name; } checks[] = {
        { 0x0000, 0x2285, "Device ID             " },
        { 0x9826, 0x0025, "PAL preset     0x9826 " },
        { 0xC858, 0x0009, "Scan mode      0xC858 " },
        { 0xC838, 0x0000, "Orientation    0xC838 " },
        { 0xC881, 0x3232, "Flicker freq   0xC881 " },
        { 0xC96C, 0x0000, "Output format  0xC96C " },
        { 0xC972, 0x0005, "Parallel ctrl  0xC972 " },
        { 0x001E, 0x0200, "Pad slew       0x001E " },
        //{ 0xC86C, 0x0001, "AE mode        0xC86C " },
        //{ 0xC86E, 0x003C, "AE target      0xC86E " },
        //{ 0xC8BE, 0x006D, "AWB L R/G      0xC8BE " },
        //{ 0xC8C0, 0x00E6, "AWB L B/G      0xC8C0 " },
        //{ 0xC8C2, 0x008C, "AWB M R/G      0xC8C2 " },
        //{ 0xC8C4, 0x009D, "AWB M B/G      0xC8C4 " },
        //{ 0xC8C6, 0x00B3, "AWB R R/G      0xC8C6 " },
        //{ 0xC8C8, 0x0073, "AWB R B/G      0xC8C8 " },
        //{ 0xC8CA, 0x0B28, "CCM L ctemp    0xC8CA " },
        //{ 0xC8FF, 0x0003, "AWB mode       0xC8FF " },
        //{ 0xA404, 0x0001, "AE algorithm   0xA404 " },
    };

    uint8_t pass = 1;
    for (int i = 0; i < (int)(sizeof(checks)/sizeof(checks[0])); i++) {
        CAM_ReadReg(i2c_addr, checks[i].addr, &readback);
        const char *status = (readback == checks[i].expected) ? "OK " : "FAIL";
        if (readback != checks[i].expected) pass = 0;
        sprintf(log_buf, "  %s  read=0x%04X  exp=0x%04X  %s\r\n",
                checks[i].name, readback, checks[i].expected, status);
        Log(log_buf);
    }

    if (!pass) {
        Log("WARNING: One or more register readbacks failed.\r\n");
        Log("         Check I2C reliability and register address mapping.\r\n");
    } else {
        Log("All register readbacks passed.\r\n");
    }

    /* Additional diagnostic: read AWB runtime gains.
     * These are read-only status registers updated by the AWB algorithm.
     * Both should differ from 0x0080 (unity) within a few seconds of
     * running under any real illumination, confirming AWB is active.
     * If they remain at 0x0080 indefinitely, AWB is not converging. */
    uint16_t r_gain = 0, b_gain = 0;
    CAM_ReadReg(i2c_addr, 0xAC12, &r_gain);
    CAM_ReadReg(i2c_addr, 0xAC14, &b_gain);

    uint32_t r_milli = ((uint32_t)r_gain * 1000U) / 128U;
    uint32_t b_milli = ((uint32_t)b_gain * 1000U) / 128U;

    sprintf(log_buf, "  AWB R gain 0xAC12 = 0x%04X (%u/128 = %lu.%03lu) [RO]\r\n",
            r_gain, r_gain,
            r_milli / 1000U,
            r_milli % 1000U);
    Log(log_buf);

    sprintf(log_buf, "  AWB B gain 0xAC14 = 0x%04X (%u/128 = %lu.%03lu) [RO]\r\n",
            b_gain, b_gain,
            b_milli / 1000U,
            b_milli % 1000U);
    Log(log_buf);

    /* Read current AWB color temperature estimate (read-only in auto AWB mode) */
    uint16_t color_temp = 0;
    CAM_ReadReg(i2c_addr, 0xC8E6, &color_temp);
    sprintf(log_buf, "  AWB color temp 0xC8E6 = %u K\r\n", color_temp);
    Log(log_buf);


    /* =========================================================================
     * STAGE 7 — Convergence stabilization delay
     *
     * The sensor is now streaming with correct configuration. AE and AWB are
     * both running and converging from their reset states.
     *
     * AE convergence: ~3–6 frames to reach target luma at 50fps = 60–120ms
     * AWB convergence: ~10–25 frames depending on scene = 200–500ms
     *
     * The application should not consume frames until both have stabilized.
     * 500ms provides ~25 frames, sufficient for AE in most scenes. AWB may
     * take longer in challenging lighting (mixed sources, low saturation scenes).
     *
     * If frames are being captured immediately after init and show a color
     * cast that fades over several seconds, increase this delay to 1000ms.
     * ========================================================================= */

    HAL_Delay(500);
    Log("CAM_Init complete. Sensor streaming.\r\n");

    return HAL_OK;
}

HAL_StatusTypeDef Photo_CaptureRaw(uint8_t  slot,
                                   uint16_t designator,
                                   uint8_t  *opcode)
{
	char log_buf[64];
    if (slot >= RAW_PHOTO_COUNT) return HAL_ERROR;

    volatile raw_photo_t *buf = RAW_BUFFER(slot);

    /* Fill header */
    buf->designator    = designator;
    uint32_t uptime = (board_status.uptime_session & 0xFFFF0000) | (board_status.uptime_session & 0x0000FFFF);
    uptime /= 1000; 	// Pass to seconds (rounded)
    buf->timestamp_MSB = (uint16_t)(board_status.uptime_total >> 16) + (uint16_t)(uptime >> 16);
    buf->timestamp_LSB = (uint16_t)(board_status.uptime_total & 0xFFFF) + (uint16_t)(uptime & 0xFFFF);

    // Saved in uint16_t for memory alignment. Some bytes wasted, but negligible
    for (int i = 0; i < OPCODE_SIZE; i++) {
        buf->opcode[i] = (uint16_t)opcode[i];
    }

    /* Arm DCMI */
    dcmi_frame_ready = 0;
    dcmi_error = 0;

    // Pre-configure DMA first
    HAL_DMA_Start_IT(hdcmi.DMA_Handle,
                     (uint32_t)&DCMI->DR,
                     (uint32_t)&buf->data[0],
                     H * L / 2);

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

	buf->black_pixels_LSB = 0;
	buf->black_pixels_MSB = 0;

    Log("DCMI: frame captured OK\r\n");
    return HAL_OK;
}

void InitCamParams(void)
{
	cam_params.ae_rule_algo_val = 0x0003; 		// TODO: Untested. Determine default for this
	return;
}

uint8_t CompressRawPhoto(uint8_t buffer, int quality)
{
	volatile compressed_photo_t *compression = COMPRESSED_BUFFER(0);
	volatile raw_photo_t *raw = RAW_BUFFER(buffer);

	char log_buf[64];

	uint32_t compression_size = 0;				// Will indicate size of compression

    // Saved in uint16_t for memory alignment. Some bytes wasted, but negligible
    for (int i = 0; i < OPCODE_SIZE; i++) {
        compression->opcode[i] = raw->opcode[i];
    }

	compression->index = (board_status.compressions_done);
	compression->designator = raw->designator;
	compression->quality = quality;

	compression->timestamp_LSB = raw->timestamp_LSB;
	compression->timestamp_MSB = raw->timestamp_MSB;


	//  PARAMETERS
	//      memory_buffer:      pointer to memory buffer where JPEG will be written
	//      buffer_size:        size of the memory buffer in bytes
	//      bytes_written:      pointer to uint32_t that will receive the number of bytes written
	//      quality:            3: Highest. Compression varies wildly (between 1/3 and 1/20).
	//                          2: Very good quality. About 1/2 the size of 3.
	//                          1: Noticeable. About 1/6 the size of 3, or 1/3 the size of 2.
	//      width, height:      image size in pixels
	//      num_components:     Must be 3 for YUV422 input (already handled internally)
	//      src_data:           pointer to YUV422 pixel data [Y0,Cb,Y1,Cr,...]
	//
	//  RETURN:
	//      0 on error. 1 on success.

	sprintf(log_buf, "Before encode: compression_size=%lu\r\n", compression_size);
	Log(log_buf);

	int ret = tje_encode_to_memory((uint8_t*)compression->data,
							 sizeof(compression->data),
	                         &compression_size,
	                         quality,
	                         L,
	                         H,
	                         3,
	                         (const unsigned char*)raw->data);

	sprintf(log_buf, "After encode: ret=%d compression_size=%lu\r\n", ret, compression_size);
	Log(log_buf);

	compression->size_MSB = (uint16_t)(compression_size >> 16);
	compression->size_LSB = (uint16_t)(compression_size & 0xFFFF);

	// Record number of black pixels from raw. Unused (0x00000000) if no black filtering was performed
	compression->black_pixels_LSB = raw->black_pixels_LSB;
	compression->black_pixels_MSB = raw->black_pixels_MSB;

	if (ret == 0){
		Log("Compression failed!\r\n");
		return 0;
	}

	Log("Compression successful!\r\n");

	return 1;
}

uint32_t count_black_pixels_uyvy(volatile uint8_t *buffer,
                                  	   uint32_t  num_pixels,
									   uint8_t   black_threshold)
{
    uint32_t black_count = 0;
    uint32_t num_words   = num_pixels / 2;  // each uint16_t holds one U/V + one Y

    for (uint32_t i = 0; i < num_words; i++) {
        // UYVY: [U0][Y0][V0][Y1] — Y bytes are at offsets 1 and 3
        uint8_t y0 = buffer[i * 4 + 1];
        uint8_t y1 = buffer[i * 2 + 3];

        if (y0 <= BLACK_THRESHOLD) black_count++;
        if (y1 <= BLACK_THRESHOLD) black_count++;
    }

    return black_count;
}

HAL_StatusTypeDef Photo_CaptureRawBlack(uint8_t   slot,
                                         uint16_t  designator,
                                         uint8_t  *opcode,
                                         uint8_t   tries,
                                         uint8_t   black_fraction)
{
    char log_buf[64];
    volatile raw_photo_t *buf = RAW_BUFFER(slot);

    for (uint8_t attempt = 1; attempt <= tries; attempt++) {

        sprintf(log_buf, "Black filter: attempt %d/%d\r\n", attempt, tries);
        Log(log_buf);

        HAL_StatusTypeDef ret = Photo_CaptureRaw(slot, designator, opcode);
        if (ret != HAL_OK) {
            sprintf(log_buf, "Capture failed on attempt %d, ret=%d\r\n", attempt, ret);
            Log(log_buf);
            return ret;  // propagate HAL_ERROR or HAL_TIMEOUT immediately
        }

        uint32_t total_pixels = H*L;
        uint32_t black_pixels = count_black_pixels_uyvy(
            (volatile uint8_t *)buf->data,
            total_pixels,
            BLACK_THRESHOLD
        );

        // Store black pixel count in the frame header (MSB/LSB)
        buf->black_pixels_MSB = (black_pixels >> 8) & 0xFF;
        buf->black_pixels_LSB =  black_pixels       & 0xFF;


        // There are less black pixels than the maximum allowed
        if (black_pixels <= black_fraction) {		// TODO: FIX!
            Log("Frame passed black filter\r\n");
            return HAL_OK;
        }

        Log("Frame rejected: too black\r\n");
        board_status.images_rejected_black++;

        if (attempt == tries) {
            Log("All attempts rejected, giving up\r\n");
            return HAL_ERROR;
        }
    }

    return HAL_ERROR;  // unreachable, satisfies compiler
}
