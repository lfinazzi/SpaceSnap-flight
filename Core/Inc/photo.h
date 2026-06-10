#ifndef __PHOTO_H__
#define __PHOTO_H__

#include "command.h"
#include "stm32f2xx_hal.h"

#include <stdint.h>

#define L 						(640U)			// Raw image length
#define H 						(480U)			// Raw image height

#define CAM_RESET_PORT  		GPIOC
#define CAM_RESET_PIN    		GPIO_PIN_0

#define CAM_I2C_ADDR_A  		(0x5D << 1)   	// 0xBA, HAL uses 8-bit shifted
#define CAM_I2C_ADDR_B  		(0x48 << 1)   	// 0x90, HAL uses 8-bit shifted

#define CAM_I2C_TIMEOUT			(100U)		  	// Timeout for I2C2
#define DCMI_TIMEOUT 			(500U)			// Timeout for DCMI interface, expected transfer time is approx 20 ms

typedef struct __attribute__((packed)){
	uint16_t designator;			  			// global raw photo number taken
	uint8_t opcode[OPCODE_SIZE]; 				// opcodes sent to take picture
    uint8_t  _pad;             					// explicit padding byte to keep alignment
	uint16_t timestamp_MSB;			      		// timestamp is uint32_t
	uint16_t timestamp_LSB;
	uint16_t data[L*H];               			// Image data in YCbCr 4:2:2 format
} raw_photo_t;

typedef struct {
	uint16_t ae_rule_algo_val; 					// Algorithm for auto exposure
} cam_params_t;		// TODO: These settings will be the ones that can be changed. Other settings? For now, only exposure considered

// TODO: Fix this static assert
typedef char static_assert_raw_photo_t_size[	// Static assert that a complete photo size is as expected, number left explicit on purpose
    (sizeof(raw_photo_t) == 614412) ? 1 : -1
];

// Aligned for 16b (SRAM). Will it be okay for 8b (FRAM)? TODO: Check
typedef struct __attribute__((packed)){
	uint16_t index;					  			// index of compressed photo
	uint16_t *address;			  	 			// memory address start for picture
	uint16_t size_MSB;			      			//compression size is uint32_t
	uint16_t size_LSB;
	uint16_t timestamp_MSB;			      		// timestamp is uint32_t
	uint16_t timestamp_LSB;
	uint8_t opcode[OPCODE_SIZE]; 				// opcodes sent to take picture
	uint8_t  _pad;             					// explicit padding byte to keep alignment
} compressed_metadata_t;

typedef struct __attribute__((packed)) {
    uint16_t *data;  							// compressed photo data
} compressed_photo_t;


/********************************************************************************
 * @brief  Asserts the shared camera reset line (RESET_BAR) LOW.
 *
 * @note   RESET_BAR is shared between Camera A and Camera B (PC0, active LOW).
 *         Asserting reset stops sensor firmware execution and causes the sensor
 *         to stop driving VREG_BASE, which in turn stops the BSR16 transistor
 *         and begins collapsing the internally regulated 1.8V VDD rail.
 *         Must be called before enabling or disabling power rails to ensure
 *         clean power sequencing and avoid ESD structure forward-biasing.
 ********************************************************************************/
void CAM_ResetAssert(void);

/********************************************************************************
 * @brief  Releases the shared camera reset line (RESET_BAR) HIGH.
 *
 * @note   RESET_BAR is shared between Camera A and Camera B (PC0, active LOW).
 *         Releasing reset starts the sensor boot sequence. The sensor internal
 *         voltage regulator begins driving VREG_BASE, turning on the external
 *         BSR16 BJT pass transistor to generate the 1.8V VDD core supply from
 *         the 2.8V VAA rail. A stabilisation delay of at least 150ms must
 *         follow this call before any I2C communication is attempted, to allow
 *         the 1.8V rail to settle and the sensor firmware to complete its
 *         Host Config Mode boot sequence.
 *         EXTCLK (27 MHz, TIM11 CH1 PWM) must be running before this call.
 ********************************************************************************/
void CAM_ResetRelease(void);

/********************************************************************************
 * @brief  Full power-up and initialisation sequence for Camera A.
 *
 * @note   Performs the following sequence in order:
 *           1. Asserts RESET_BAR LOW (shared with Camera B).
 *           2. Enables the 2.8V power rail via IMG_ENA_A (PA2), which supplies
 *              VAA, VAA_PIX, VDD_IO, VDD_DAC and VDD_PLL. Waits 10ms for
 *              rails to stabilise.
 *           3. Enables the I2C level shifter via IMG_ENA (PA12) then
 *              IMG_I2C_ENA (PA11).
 *           4. Releases RESET_BAR, starting the sensor boot sequence and
 *              enabling the internal BSR16-based 1.8V VDD regulator.
 *           5. Waits 150ms for the 1.8V rail to stabilise and the sensor
 *              firmware to complete boot in Host Config Mode.
 *         After this call returns, the sensor is ready for I2C register access.
 *         EXTCLK (27 MHz via TIM11 CH1) must be running before this call.
 *         Camera B must not be active simultaneously due to shared RESET_BAR
 *         and shared I2C bus address (0x48).
 ********************************************************************************/
void ActivateCAMA(void);

/********************************************************************************
 * @brief  Full power-up and initialisation sequence for Camera B.
 *
 * @note   Identical sequence to ActivateCAMA() but uses IMG_ENA_B (PA3) to
 *         enable the Camera B 2.8V power rail instead of IMG_ENA_A (PA2).
 *         All other signals (RESET_BAR, IMG_I2C_ENA, IMG_ENA) are shared with
 *         Camera A. Camera A must be fully deactivated before calling this
 *         function to avoid I2C address conflicts and undefined behaviour on
 *         the shared RESET_BAR line.
 *         After this call returns, the sensor is ready for I2C register access.
 *         EXTCLK (27 MHz via TIM11 CH1) must be running before this call.
 ********************************************************************************/
void ActivateCAMB(void);

/********************************************************************************
 * @brief  Full power-down sequence for Camera A.
 *
 * @note   Performs the following sequence in order:
 *           1. Asserts RESET_BAR LOW, stopping sensor firmware and halting
 *              VREG_BASE drive. The BSR16 begins turning off and the 1.8V VDD
 *              rail starts to collapse. Waits 5ms to allow 1.8V to begin
 *              collapsing before the 2.8V rail is removed, preventing reverse
 *              biasing of internal ESD protection structures.
 *           2. Disables the I2C level shifter via IMG_I2C_ENA (PA11) then
 *              IMG_ENA (PA12), isolating the I2C bus before the 2.8V rail
 *              collapses to prevent spurious bus transactions from floating
 *              lines.
 *           3. Disables the 2.8V rail via IMG_ENA_A (PA2).
 *           4. Waits 100ms for the 4.7uF reservoir capacitor on the 1.8V node
 *              and bulk capacitors on the 2.8V rail to fully discharge before
 *              any re-enable attempt.
 *         Safe to call even if ActivateCAMA() was not previously called.
 ********************************************************************************/
void DeactivateCAMA(void);

/********************************************************************************
 * @brief  Full power-down sequence for Camera B.
 *
 * @note   Identical sequence to DeactivateCAMA() but disables the Camera B
 *         2.8V rail via IMG_ENA_B (PA3) instead of IMG_ENA_A (PA2).
 *         RESET_BAR, IMG_I2C_ENA and IMG_ENA are shared with Camera A and
 *         will be affected by this call regardless of which camera was active.
 *         After this call returns, both cameras are unpowered and RESET_BAR
 *         is asserted. A minimum of 100ms must elapse before re-enabling
 *         either camera to ensure full rail discharge.
 ********************************************************************************/
void DeactivateCAMB(void);

/********************************************************************************
 * @brief  Polls the Host Command Interface doorbell bit until the firmware
 *         completes processing the last issued command.
 *
 * @note   After issuing any HCI command via register 0x0040, the firmware
 *         sets bit 15 (doorbell) of the command register. This function
 *         polls until bit 15 clears, indicating command completion.
 *         The lower byte of the command register is logged for debugging
 *         but not checked for errors at this stage.
 *         Timeout is set to 200ms — sufficient for all known HCI commands.
 *
 * @param  i2c_addr   7-bit I2C address shifted left by 1 (HAL format).
 *                    Use CAM_I2C_ADDR_A or CAM_I2C_ADDR_B.
 *
 * @retval HAL_OK     Doorbell cleared, command completed successfully.
 * @retval HAL_TIMEOUT Firmware did not respond within 200ms.
 ********************************************************************************/
HAL_StatusTypeDef CAM_WaitDoorbell(uint8_t i2c_addr);

/* ─────────────────────────────────────────────────────────────────────────────
 * Internal helper: issue a Change-Config command and poll until complete.
 * Change-Config is required after writing frame-start-synchronized variables
 * (scan mode, orientation, port configuration, AE/AWB parameters).
 * The SOC applies the new settings on the next frame boundary.
 * ───────────────────────────────────────────────────────────────────────────── */
HAL_StatusTypeDef CAM_ChangeConfig(uint8_t i2c_addr);

/********************************************************************************
 * @brief  Queries the current system state of the camera via the Host
 *         Command Interface (HCI).
 *
 * @note   Issues the HC_SYSMGR_GET_STATE command (0x8101) to register 0x0040
 *         and waits for the firmware to complete processing via the doorbell
 *         mechanism. The result is read back from the parameter pool at 0xFC00.
 *
 *         Known state values:
 *         0x2800 = SYS_STATE_ENTER_CONFIG_CHANGE (confirmed, datasheet p.23)
 *         0x3100 = observed post-boot state (unconfirmed, assumed streaming)
 *         Other values may be returned — log and investigate empirically.
 *
 *         If the doorbell times out, 0xFFFF is returned as an error sentinel
 *         value that cannot be confused with a valid state.
 *
 * @param  i2c_addr   7-bit I2C address shifted left by 1 (HAL format).
 *                    Use CAM_I2C_ADDR_A or CAM_I2C_ADDR_B.
 *
 * @retval uint16_t   Current system state value read from parameter pool.
 *                    0xFFFF if doorbell timed out.
 ********************************************************************************/
uint16_t CAM_GetState(uint8_t i2c_addr);

/********************************************************************************
 * @brief  Writes a 16-bit value to a 16-bit addressed register on the
 *         ASX340AT image sensor over I2C.
 *
 * @note   Internal helper — not exposed in photo.h. All sensor register
 *         access within photo.c should go through this function.
 *         The ASX340AT register bus uses 16-bit addresses and 16-bit data.
 *         The value is transmitted big-endian (MSB first) as required by
 *         the sensor protocol. Uses hi2c2 with a 100ms timeout.
 *         Do not call this function before ActivateCAMA() or ActivateCAMB()
 *         has returned, as the I2C level shifter will not be enabled.
 *
 * @param  i2c_addr   8-bit shifted I2C address of the target sensor.
 *                    Use CAM_I2C_ADDR_A (0xBA) or CAM_I2C_ADDR_B (0x90).
 * @param  reg        16-bit register address to write to.
 * @param  val        16-bit value to write.
 *
 * @retval HAL_OK     Register write completed successfully.
 * @retval HAL_ERROR  I2C transaction failed (bus error, NACK, timeout).
 ********************************************************************************/
HAL_StatusTypeDef CAM_WriteReg(uint8_t i2c_addr, uint16_t reg, uint16_t val);


/********************************************************************************
 * @brief  Reads a 16-bit value from a 16-bit addressed register on the
 *         ASX340AT image sensor over I2C.
 *
 * @note   Internal helper — not exposed in photo.h. All sensor register
 *         access within photo.c should go through this function.
 *         The ASX340AT register bus uses 16-bit addresses and 16-bit data.
 *         The response is read big-endian (MSB first) and reassembled into
 *         a uint16_t. Uses hi2c2 with a 100ms timeout.
 *         Do not call this function before ActivateCAMA() or ActivateCAMB()
 *         has returned, as the I2C level shifter will not be enabled.
 *         On I2C failure the value at *val is not modified.
 *
 * @param  i2c_addr   8-bit shifted I2C address of the target sensor.
 *                    Use CAM_I2C_ADDR_A (0xBA) or CAM_I2C_ADDR_B (0x90).
 * @param  reg        16-bit register address to read from.
 * @param  val        Pointer to uint16_t where the result will be stored.
 *
 * @retval HAL_OK     Register read completed successfully, *val is valid.
 * @retval HAL_ERROR  I2C transaction failed (bus error, NACK, timeout).
 *                    *val is unchanged.
 ********************************************************************************/
HAL_StatusTypeDef CAM_ReadReg(uint8_t i2c_addr, uint16_t reg, uint16_t *val);

/********************************************************************************
 * @brief  Initialises the camera sensor for Raw Bayer progressive output
 *         on the parallel digital port.
 *
 * @note   Configures the following in two Change-Config stages:
 *
 *         Stage 1 — NTSC progressive parallel preset:
 *           Writes 0x9426 = 0x0025 to enable VGA progressive output on the
 *           parallel port using the NTSC driver preset. A Change-Config is
 *           issued to apply this before proceeding.
 *
 *         Stage 2 — Output format and parallel port control:
 *           0xC96C = 0x0200  Raw Bayer output format (bits[9:8] = 10)
 *           0xC972 = 0x0005  Parallel port enabled, progressive mode,
  *                           continuous PIXCLK (bit[4]=0, bit[2:1]=10, bit[0]=1)
 *           A second Change-Config is issued to apply these settings.
 *
 *         Register addresses 0xC96C and 0xC972 are CamControl variables
 *         confirmed in the ASX340AT Developer Guide.
 *         Register 0x9426 is the NTSC driver variable confirmed in the
 *         ASX340AT Developer Guide.
 *
 *         This function must be called after ActivateCAMx() has completed
 *         and the sensor has finished its boot sequence.
 *
 * @param  i2c_addr   7-bit I2C address shifted left by 1 (HAL format).
 *                    Use CAM_I2C_ADDR_A or CAM_I2C_ADDR_B.
 *
 * @retval HAL_OK       Configuration applied successfully.
 * @retval HAL_TIMEOUT  Doorbell did not clear within 200ms on either stage.
 ********************************************************************************/
HAL_StatusTypeDef CAM_Init(uint8_t i2c_addr);

/********************************************************************************
 * @brief  Captures a single raw frame from the active camera into an SRAM
 *         raw buffer slot via DCMI DMA in snapshot mode.
 *
 * @note   Writes the frame header (designator, opcode, timestamp) to the
 *         target raw_photo_t struct in SRAM before starting the DMA transfer.
 *         Then calls HAL_DCMI_Start_DMA() in DCMI_MODE_SNAPSHOT pointing
 *         directly at the data[] field of the target struct. The DCMI waits
 *         for the next VSYNC edge from the sensor before capturing exactly
 *         one frame. DMA transfers 32-bit words directly to FSMC SRAM.
 *
 *         Completion is signalled by HAL_DCMI_FrameEventCallback() setting
 *         dcmi_frame_ready = 1. This function polls that flag with a 500ms
 *         timeout, kicking the IWDG on each iteration. On timeout or DMA
 *         error, HAL_DCMI_Stop() is called and the function returns an
 *         error code.
 *
 *         The sensor must be powered, configured and streaming before calling
 *         this function. ASX340AT_Init() must have completed successfully.
 *         DCMI and DMA2 must be initialised (MX_DMA_Init, MX_DCMI_Init).
 *
 * @param  slot        Raw buffer slot index (0, 1 or 2). Selects the target
 *                     address via RAW_BUFFER(slot). Must be < RAW_PHOTO_COUNT.
 * @param  designator  Global photo sequence number written into the header.
 * @param  opcode      Pointer to OPCODE_SIZE bytes written into the header.
 *
 * @retval HAL_OK      Frame captured successfully. raw_photo_t at
 *                     RAW_BUFFER(slot) contains valid header and pixel data.
 * @retval HAL_ERROR   Invalid slot, DCMI failed to start, or DMA error
 *                     reported by HAL_DCMI_ErrorCallback().
 * @retval HAL_TIMEOUT Frame not completed within 500ms. Sensor may not be
 *                     streaming or VSYNC signal absent.
 ********************************************************************************/
HAL_StatusTypeDef Photo_CaptureRaw(uint8_t  slot, uint16_t designator, uint8_t  *opcode);

/********************************************************************************
 * @brief  Function to initialize changable CAM params default values
 *
 * TODO: Add other params?
 ********************************************************************************/
void InitCamParams(void);

/********************************************************************************
 * @brief  Function to compress photo in raw buffer and save to SRAM + FRAM
 *
 *	returns 0 if compression failed and 1 if it was successful
 *
 * TODO: Comment pending
 ********************************************************************************/
uint8_t CompressRawPhoto(uint8_t buffer);

#endif
