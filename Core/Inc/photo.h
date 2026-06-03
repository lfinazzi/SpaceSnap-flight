#ifndef __PHOTO_H__
#define __PHOTO_H__

#include "command.h"
#include "stm32f2xx_hal.h"

#include <stdint.h>

#define L 						(480U)			// Raw image length
#define H 						(640U)			// Raw image height

#define CAM_RESET_PORT  		GPIOC
#define CAM_RESET_PIN    		GPIO_PIN_0

#define CAM_I2C_ADDR_A  		(0x5D << 1)   	// 0xBA, HAL uses 8-bit shifted
#define CAM_I2C_ADDR_B  		(0x48 << 1)   	// 0x90, HAL uses 8-bit shifted

#define I2C_TIMEOUT				(100U)		  	// Timeout for I2C2


typedef struct __attribute__((packed)){
	uint16_t designator;			  			// global raw photo number taken
	uint8_t opcode[OPCODE_SIZE]; 				// opcodes sent to take picture
    uint8_t  _pad;             					// explicit padding byte to keep alignment
	uint16_t timestamp_MSB;			      		// timestamp is uint32_t
	uint16_t timestamp_LSB;
	uint16_t data[L*H];               			// Image data in YCbCr 4:2:2 format
} raw_photo_t;

typedef char static_assert_raw_photo_t_size[	// Static assert that a complete photo size is as expected, number left explicit on purpose
    (sizeof(raw_photo_t) == 614412) ? 1 : -1
];

typedef struct {
	uint16_t index;					  			// index of compressed photo
	uint16_t *address;			  	 			// memory address start for picture
	uint32_t size;				 	  			// size of compressed photo
	uint32_t timestamp;				  			// internal timestamp
	uint16_t opcode[OPCODE_SIZE];				// instruction + opcode, saved in 16b to avoid padding
} compressed_metadata_t;

typedef struct {
    uint16_t *data;  						// compressed photo data
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
 * @brief  Verifies I2C communication with the ASX340AT sensor by performing
 *         two read-only checks against known register values.
 *
 * @note   Performs the following checks in order:
 *           1. Reads the device ID register (0x0000) and verifies it matches
 *              0x2285. This confirms the sensor is powered, the I2C bus is
 *              functional, and the correct device is present at i2c_addr.
 *           2. Reads the firmware version register (0x001C) and logs the
 *              result. No pass/fail condition on firmware version — any
 *              non-zero readable value confirms the sensor firmware is running.
 *         Both checks are read-only — no register writes are performed,
 *         making this function safe to call immediately after boot without
 *         disturbing the Auto-Config state the sensor enters on power-up
 *         due to the floating SPI_SDI pin.
 *         Must be called after ActivateCAMA() or ActivateCAMB() has returned.
 *
 * @param  i2c_addr   8-bit shifted I2C address of the target sensor.
 *                    Use CAM_I2C_ADDR_A (0xBA) or CAM_I2C_ADDR_B (0x90).
 *
 * @retval HAL_OK     Both checks passed. Sensor is alive and responding.
 * @retval HAL_ERROR  I2C transaction failed or device ID did not match 0x2285.
 ********************************************************************************/
HAL_StatusTypeDef Camera_CommsTest(uint8_t i2c_addr);


/********************************************************************************
 * @brief  Configures the ASX340AT sensor with default parameters for
 *         image capture after boot.
 *
 * @note   The sensor arrives here in Auto-Config Mode (NTSC streaming) due
 *         to the floating SPI_SDI pin on the sensor board. This function:
 *           1. Verifies the sensor is alive over I2C.
 *           2. Issues a soft reset via register 0x001A to return all
 *              registers to default values and stop streaming. The I2C
 *              bus remains active through soft reset unlike hard reset.
 *           3. Waits for the sensor to complete its internal re-init.
 *           4. Verifies the sensor is still alive after soft reset.
 *           5. Configures slew rate, output format, parallel port,
 *              scan mode and auto exposure parameters.
 *           6. Issues a Change-Config HCI command to apply all settings
 *              and restart streaming with the new configuration.
 *           7. Polls the DOORBELL bit of COMMAND_REGISTER (0x0040) until
 *              the command is acknowledged or a 1000ms timeout elapses.
 *         Must be called after Camera_CommsTest() has returned HAL_OK.
 *         Do not call this function without first calling ActivateCAMA()
 *         or ActivateCAMB() as the sensor must be powered and out of reset.
 *
 * @param  i2c_addr   8-bit shifted I2C address of the target sensor.
 *                    Use CAM_I2C_ADDR_A (0xBA) or CAM_I2C_ADDR_B (0x90).
 *
 * @retval HAL_OK      Sensor configured and streaming successfully.
 * @retval HAL_ERROR   I2C transaction failed or device ID mismatch.
 * @retval HAL_TIMEOUT Change-Config command not acknowledged within 1000ms.
 ********************************************************************************/
HAL_StatusTypeDef ASX340AT_Init(uint8_t i2c_addr);

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

#endif
