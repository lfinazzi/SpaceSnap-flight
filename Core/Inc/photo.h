/**
  ******************************************************************************
  * @file           : photo.h
  * @brief          : Camera driver interface — capture API and configuration types
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
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

extern volatile uint8_t dcmi_frame_ready;
extern volatile uint8_t dcmi_error;

/* DEFAULTS for black filtering and advanced photo settings */

// These settings give ~18-19 counts for black pixels (lens cap on)
// TODO TEST: These settings must be determined when testing under representative conditions
#define BLACK_THRESHOLD_DEFAULT	(24U)			// Min. intensity to consider a pixel non-dark (out of 256), default value
#define GAIN_ANALOG_DEFAULT		(128U)			// Default sensor analog gain for advanced mode, 32 is unity
#define GAIN_DIGITAL_DEFAULT	(128U)			// Default sensor digital gain for advanced mode, 128 is unity
#define EXPOSURE_COARSE_DEFAULT	(4U)			// Default sensor exposure for advanced mode, each count is PAL freq (@ 54 MHz) * 13.5 MHz / 54 MHz = 256.0 us (PAL)
#define EXPOSURE_FINE_DEFAULT	(0U)			// Default sensor exposure for advanced mode, each count is 1 / 13.5 MHz = 0.074 us per clock (PAL or NTSC)

/* DEFAULTS for delayed picture bursts */

#define BURST_NUM_PHOTOS 		(4U)			// Fill 4 of 5 available buffers
#define BURST_INTERVAL			(10U)			// Take pictures with no delay in between (in seconds)
#define BURST_COMPRESSION		(1U)			// Perform compressions of all pictures taken
#define BURST_COMPR_QUALITY		(1U)			// Worst quality to maximize memory

typedef struct __attribute__((packed)){
	uint16_t num_photos;						// number of photos to take after delay. Must be less than RAW_PHOTO_COUNT (5 in this case)
	uint16_t time_between_photos;				// Time between photos taken in a burst
	uint16_t perform_compressions;				// Perform compressions and save to FRAM after taking these pictures?
	uint16_t compression_quality;				// Compression quality (1, 2, or 3)
} delayed_params_t;


typedef struct __attribute__((packed)){
	uint16_t designator;			  			// global raw photo number taken
	uint16_t opcode[OPCODE_SIZE]; 				// opcodes sent to take picture
	uint16_t timestamp_MSB;			      		// timestamp is uint32_t
	uint16_t timestamp_LSB;
	uint16_t black_pixels_MSB;					// Black pixels in image
	uint16_t black_pixels_LSB;
	uint16_t data[L*H];               			// Image data in YCbCr 4:2:2 format
} raw_photo_t;

typedef char static_assert_raw_photo_t_size[	// Static assert that a complete photo size is as expected, number left explicit on purpose
    (sizeof(raw_photo_t) == 614420) ? 1 : -1
];

typedef struct __attribute__((packed)) {
	uint16_t black_threshold;					// Threshold to consider a pixel black with black filtering enabled
	uint16_t sensor_analog_gain; 				// Fixed analog gain for advanced mode
	uint16_t sensor_digital_gain; 				// Fixed digital gain for advanced mode
	uint16_t sensor_coarse_exposure;			// Coarse exposure for advanced mode
	uint16_t sensor_fine_exposure;			// Fine exposure for advanced mode
} cam_params_t;									// These settings are the ones that can be changed for CameraParams
// On adding more methods, remember to initialize with default macro values and add in CMD_ChangeCamParams() to be able to change them


// Aligned for 16b (SRAM)
typedef struct __attribute__((packed)) {
	uint16_t index;					  			// index of compressed photo
	uint16_t designator;			  			// from which global raw photo number compression was done
	uint16_t opcode[OPCODE_SIZE]; 				// opcodes sent to take original picture
    uint16_t quality;             				// Compression quality
	uint16_t size_MSB;			      			// Compression size is uint32_t
	uint16_t size_LSB;
	uint16_t timestamp_MSB;			      		// timestamp is uint32_t (timestamp of original picture)
	uint16_t timestamp_LSB;
	uint16_t black_pixels_MSB;					// Black pixels in image
	uint16_t black_pixels_LSB;
	uint8_t  data[2*L*H];    					// Image data in YCbCr 4:2:2 format, at least as big as raw photo for different qualities
} compressed_photo_t;

typedef char static_assert_compressed_photo_t_size[	// Static assert that a compressed photo size is as expected, number left explicit on purpose
    (sizeof(compressed_photo_t) == 614428) ? 1 : -1
];


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
 *           1. Enables IMG_ENA (PA12) to power the level shifter supply rail.
 *           2. Starts EXTCLK (27 MHz via TIM11 CH1 PWM). EXTCLK must be
 *              running before the sensor power rails come up.
 *           3. Asserts RESET_BAR LOW (shared with Camera B).
 *           4. Enables the 2.8V power rail via IMG_ENA_A (PA2). Waits 10ms
 *              for rails to stabilise.
 *           5. Enables the I2C level shifter via IMG_I2C_ENA (PA11).
 *           6. Releases RESET_BAR, starting the sensor boot sequence and
 *              enabling the internal BSR16-based 1.8V VDD regulator.
 *           7. Waits 150ms for the 1.8V rail to stabilise and the sensor
 *              firmware to complete boot in Host Config Mode.
 *         After this call returns, the sensor is ready for I2C register
 *         access. Camera B must not be active simultaneously due to shared
 *         RESET_BAR and shared I2C bus address (0x48).
 ********************************************************************************/
void ActivateCAMA(void);


/********************************************************************************
 * @brief  Full power-up and initialisation sequence for Camera B.
 *
 * @note   Identical sequence to ActivateCAMA() but uses IMG_ENA_B (PA3) to
 *         enable the Camera B 2.8V power rail instead of IMG_ENA_A (PA2).
 *         All other signals (RESET_BAR, IMG_I2C_ENA, IMG_ENA, EXTCLK) are
 *         shared with Camera A. Camera A must be fully deactivated before
 *         calling this function to avoid I2C address conflicts and undefined
 *         behaviour on the shared RESET_BAR line.
 *         After this call returns, the sensor is ready for I2C register access.
 ********************************************************************************/
void ActivateCAMB(void);


/********************************************************************************
 * @brief  Full power-down sequence for Camera A.
 *
 * @note   Performs the following sequence in order:
 *           1. Asserts RESET_BAR LOW, stopping sensor firmware and halting
 *              VREG_BASE drive. Waits 1ms.
 *           2. Disables the I2C level shifter via IMG_I2C_ENA (PA11).
 *              Waits 1ms.
 *           3. Disables the 2.8V rail via IMG_ENA_A (PA2). Waits 1ms.
 *           4. Stops EXTCLK (TIM11 CH1 PWM).
 *           5. Waits 100ms for bulk capacitors on the 2.8V rail and the
 *              4.7uF reservoir on the 1.8V node to fully discharge before
 *              any re-enable attempt.
 *           6. Disables IMG_ENA (PA12).
 *         Safe to call even if ActivateCAMA() was not previously called.
 ********************************************************************************/
void DeactivateCAMA(void);


/********************************************************************************
 * @brief  Full power-down sequence for Camera B.
 *
 * @note   Identical sequence to DeactivateCAMA() but disables the Camera B
 *         2.8V rail via IMG_ENA_B (PA3) instead of IMG_ENA_A (PA2).
 *         RESET_BAR, IMG_I2C_ENA, IMG_ENA, and EXTCLK are shared with
 *         Camera A and will be affected by this call regardless of which
 *         camera was active. After this call returns, both cameras are
 *         unpowered and RESET_BAR is asserted. A minimum of 100ms must
 *         elapse before re-enabling either camera.
 ********************************************************************************/
void DeactivateCAMB(void);


/********************************************************************************
 * @brief  Polls the Host Command Interface doorbell bit until the firmware
 *         completes processing the last issued command.
 *
 * @note   After issuing any HCI command via register 0x0040, the host sets
 *         bit 15 (doorbell) of the command register. The sensor firmware
 *         clears bit 15 when command processing is complete. This function
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


/********************************************************************************
 * @brief  Issues a Change-Config command to the sensor and polls until the
 *         firmware acknowledges completion via the doorbell mechanism.
 *
 * @note   Required after writing any frame-start-synchronized variable
 *         (scan mode, orientation, port configuration, AE/AWB parameters).
 *         Writes 0x2800 to the parameter pool (0xFC00) and 0x8100 to the
 *         command register (0x0040), then calls CAM_WaitDoorbell(). The SOC
 *         applies the new settings on the next frame boundary.
 *
 * @param  i2c_addr  8-bit shifted I2C address of the target sensor.
 *                   Use CAM_I2C_ADDR_A or CAM_I2C_ADDR_B.
 *
 * @return HAL_OK on success.
 *         HAL_TIMEOUT if the doorbell did not clear within 200ms.
 ********************************************************************************/
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
 * @note   All sensor register access within photo.c should go through this
 *         function. The ASX340AT register bus uses 16-bit addresses and
 *         16-bit data. The value is transmitted big-endian (MSB first) as
 *         required by the sensor protocol. Uses hi2c2 with a 100ms timeout.
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
 * @note   All sensor register access within photo.c should go through this
 *         function. The ASX340AT register bus uses 16-bit addresses and
 *         16-bit data. The response is read big-endian (MSB first) and
 *         reassembled into a uint16_t. Uses hi2c2 with CAM_I2C_TIMEOUT timeout.
 *         Do not call this function before ActivateCAMA() or ActivateCAMB()
 *         has returned, as the I2C level shifter will not be enabled.
 *
 *         NOTE: *val is always written after the I2C transaction, even on
 *         HAL_ERROR. On failure *val will contain whatever bytes were
 *         partially received into the internal buffer (likely 0x0000).
 *         Callers should check the return value before using *val.
 *
 * @param  i2c_addr   8-bit shifted I2C address of the target sensor.
 *                    Use CAM_I2C_ADDR_A (0xBA) or CAM_I2C_ADDR_B (0x90).
 * @param  reg        16-bit register address to read from.
 * @param  val        Pointer to uint16_t where the result will be stored.
 *
 * @retval HAL_OK     Register read completed successfully, *val is valid.
 * @retval HAL_ERROR  I2C transaction failed. *val contains partial/zero data.
 ********************************************************************************/
HAL_StatusTypeDef CAM_ReadReg(uint8_t i2c_addr, uint16_t reg, uint16_t *val);


/********************************************************************************
 * @brief  Initialises the ASX340AT camera sensor for YCbCr 4:2:2 progressive
 *         output on the parallel digital port (640 x 480).
 *
 * @note   Configures the sensor in two active Change-Config stages:
 *
 *         Stage 1 — Scan mode, orientation, and flicker avoidance:
 *           0x9826 = 0x0025  PAL progressive preset for VGA parallel output
 *           0xC858 = 0x0009  VGA50 progressive scan (50fps, EU mains)
 *           0xC838 = 0x0000  No image flip or mirror (override Auto-Config
 *                            floating GPIO sampling)
 *           0xC881 = 0x0032  50Hz flicker avoidance (EU/ARG mains)
 *
 *         Stage 2 — Output format, parallel port, FOV alignment:
 *           0xC96C = 0x0000  YCbCr 4:2:2 UYVY output format
 *           0xC972 = 0x0005  Parallel port enabled, progressive, continuous
 *                            PIXCLK (required for STM32 DCMI hardware sync)
 *           0x001E = 0x0200  Pad slew rate control (takes effect immediately,
 *                            no Change-Config required)
 *           0xC85E = 0x02D0  720 active pixels (this is not a mistake, image still outputs in 640 x 480)
 *           0xC860 = 0x0000  Zero pixel offset
 *
 *         After Stage 2, performs a full register readback verification and
 *         logs AWB runtime gains (0xAC12, 0xAC14) and color temperature
 *         (0xC8E6) as diagnostics. Waits 500ms for AE/AWB convergence before
 *         returning.
 *
 *         Must be called after ActivateCAMx() has completed. Returns HAL_ERROR
 *         immediately if the device ID readback does not match 0x2285.
 *
 * @param  i2c_addr   8-bit shifted I2C address of the target sensor.
 *                    Use CAM_I2C_ADDR_A (0xBA) or CAM_I2C_ADDR_B (0x90).
 *
 * @return HAL_OK on success.
 *         HAL_ERROR if device ID mismatch on entry.
 *         HAL_TIMEOUT if any Change-Config doorbell poll times out.
 ********************************************************************************/
HAL_StatusTypeDef CAM_Init(uint8_t i2c_addr);


/********************************************************************************
 * @brief  Initialises the ASX340AT camera sensor for YCbCr 4:2:2 progressive
 *         output on the parallel digital port (640 x 480) with manual exposure
 *         and gain control.
 *
 * @note   Extends CAM_Init() with one additional configuration stages:
 *
 *         Stage 1 — Scan mode, orientation, and flicker avoidance:
 *           0x9826 = 0x0025  PAL progressive preset for VGA parallel output
 *           0xC858 = 0x0009  VGA50 progressive scan (50fps, EU mains)
 *           0xC838 = 0x0000  No image flip or mirror (override Auto-Config
 *                            floating GPIO sampling)
 *           0xC881 = 0x0032  50Hz flicker avoidance (EU/ARG mains)
 *
 *         Stage 2 — Output format, parallel port, FOV alignment:
 *           0xC96C = 0x0000  YCbCr 4:2:2 UYVY output format
 *           0xC972 = 0x0005  Parallel port enabled, progressive, continuous
 *                            PIXCLK (required for STM32 DCMI hardware sync)
 *           0x001E = 0x0200  Pad slew rate control (takes effect immediately,
 *                            no Change-Config required)
 *           0xC85E = 0x02D0  720 active pixels (image still outputs at 640x480)
 *           0xC860 = 0x0000  Zero pixel offset
 *
 *         Stage 3 — Manual exposure and gain (no Change-Config required):
 *           0xA804 = 0x0000  Full manual exposure (AE track disabled). Host
 *                            controls integration times and gains directly.
 *           0xC83A = cam_params.sensor_analog_gain   Analog gain (unity = 32).
 *                            Range 0.5–16x. Applied before ADC; does not
 *                            amplify quantisation noise.
 *           0xC84C = cam_params.sensor_digital_gain  Second digital gain
 *                            (unity = 128). Applied after ADC; use only if
 *                            analog gain headroom is exhausted.
 *           0xC840 = cam_params.sensor_coarse_exposure  Coarse integration
 *                            time in line periods. Each line = 256 µs at
 *                            13.5 MHz PIXCLK (PAL, 3906.25 Hz line rate).
 *                            Coarse = 1 → 256 µs. Keep below 4 lines (1 ms)
 *                            to avoid motion blur at LEO ground speeds.
 *           0xC842 = cam_params.sensor_fine_exposure  Fine integration time
 *                            in pixel clocks (1 clock = 0.074 µs at 13.5 MHz).
 *                            Use to tune exposure between coarse steps.
 *                            Max value = line_length_pck - 1 = 3455 clocks.
 *
 *         All gain and exposure values are read from board_status.cam_params,
 *         which is persisted in FRAM and updated via ground command.
 *
 *         After Stage 3, performs a full register readback verification
 *         including all exposure and gain registers, and logs AWB runtime
 *         gains (0xAC12, 0xAC14) and color temperature (0xC8E6) as
 *         diagnostics. Waits 500ms for AWB convergence before returning.
 *         Note: AE convergence delay is not required since AE is disabled,
 *         but the delay is retained for AWB stability.
 *
 *         Must be called after ActivateCAMx() has completed. Returns HAL_ERROR
 *         immediately if the device ID readback does not match 0x2285.
 *
 * @param  i2c_addr   8-bit shifted I2C address of the target sensor.
 *                    Use CAM_I2C_ADDR_A (0xBA) or CAM_I2C_ADDR_B (0x90).
 *
 * @return HAL_OK on success.
 *         HAL_ERROR if device ID mismatch on entry.
 *         HAL_TIMEOUT if any Change-Config doorbell poll times out.
 ********************************************************************************/
HAL_StatusTypeDef CAM_InitAdvanced(uint8_t i2c_addr);


/********************************************************************************
 * @brief  Captures a single raw frame from the active camera into an SRAM
 *         raw buffer slot via DCMI DMA in snapshot mode.
 *
 * @note   Writes the frame header (designator, opcode, timestamp) to the
 *         target raw_photo_t struct in SRAM before starting the DMA transfer.
 *         Timestamp is computed as board_status.uptime_total +
 *         (uptime_session / 1000) seconds, split across timestamp_MSB and
 *         timestamp_LSB fields.
 *
 *         Starts DMA manually via HAL_DMA_Start_IT() before calling
 *         HAL_DCMI_Start_DMA() in DCMI_MODE_SNAPSHOT. Resets DCMI handle
 *         state (HAL_DCMI_Stop + hdcmi.State = READY) before each capture
 *         to allow repeated captures without re-initializing the peripheral.
 *         Polls dcmi_frame_ready and dcmi_error flags with DCMI_TIMEOUT ms
 *         timeout, kicking the IWDG on each iteration. On timeout, logs DCMI
 *         and DMA status registers for diagnostics before returning HAL_TIMEOUT.
 *
 *         The sensor must be powered, configured and streaming (CAM_Init()
 *         completed successfully) before calling this function.
 *
 * @param  slot        Raw buffer slot index (0 to RAW_PHOTO_COUNT-1).
 * @param  designator  Global photo sequence number written into the header.
 * @param  opcode      Pointer to OPCODE_SIZE bytes written into the header.
 *
 * @return HAL_OK on success.
 *         HAL_ERROR if slot is invalid or DCMI failed to start.
 *         HAL_TIMEOUT if frame not completed within DCMI_TIMEOUT ms.
 ********************************************************************************/
HAL_StatusTypeDef Photo_CaptureRaw(uint8_t  slot, uint16_t designator, uint8_t  *opcode);


/********************************************************************************
 * @brief  Compresses a raw YCbCr 4:2:2 photo from SRAM into JPEG and stores
 *         the result in the SRAM compression buffer.
 *
 * @note   Reads raw pixel data from RAW_BUFFER(buffer) and writes the
 *         compressed JPEG into COMPRESSED_BUFFER(0) (only one compression
 *         buffer exists). Copies header metadata (index, designator, opcode,
 *         quality, timestamp, black_pixels) from the raw buffer into the
 *         compressed_photo_t header before encoding.
 *
 *         Uses tje_encode_to_memory() (TinyJPEG library) for encoding.
 *         Quality levels: 1 = noticeable (~1/6 size of quality 3),
 *                         2 = very good  (~1/2 size of quality 3),
 *                         3 = highest    (compression ratio varies 1/3–1/20).
 *         Output size is stored in compression->size_MSB and size_LSB.
 *
 *         black_pixels MSB/LSB are copied from the raw buffer as-is.
 *         If no black filtering was performed they will be 0x0000.
 *
 * @param  buffer   Raw buffer slot index to compress (0 to RAW_PHOTO_COUNT-1).
 * @param  quality  Compression quality level: 1 (lowest) to 3 (highest).
 *
 * @return 1 on success.
 *         0 if tje_encode_to_memory() failed.
 ********************************************************************************/
uint8_t CompressRawPhoto(uint8_t buffer, int quality);


/********************************************************************************
 * @brief  Counts the number of pixels at or below a luma threshold in a
 *         UYVY-packed YCbCr 4:2:2 buffer.
 *
 * @note   In UYVY format each 32-bit group encodes two pixels as:
 *           word[0]: bits[15:8] = Y0,  bits[7:0] = U0
 *           word[1]: bits[15:8] = Y1,  bits[7:0] = V0
 *         Only the Y (luma) bytes are examined; chroma bytes are ignored.
 *
 *         In limited-range YCbCr (ITU-R BT.601) nominal black is Y=16.
 *         For full-range output (0–255) nominal black is Y=0.
 *         A threshold of 8–16 is recommended as a starting point to capture
 *         deep-space black without clipping dark Earth features.
 *
 * @param  buffer           Pointer to UYVY pixel data. Must contain at least
 *                          num_pixels / 2 uint16_t words. num_pixels must be even.
 * @param  num_pixels       Total number of pixels in the image (width * height).
 * @param  black_threshold  Luma value (0–255) at or below which a pixel is
 *                          considered black.
 *
 * @return Number of pixels whose luma is <= black_threshold.
 ********************************************************************************/
uint32_t count_black_pixels_uyvy(volatile uint8_t *buffer, uint32_t  num_pixels, uint8_t black_threshold);


/********************************************************************************
 * @brief  Captures a raw frame with black-pixel filtering and automatic retry.
 *
 * @note   Wraps Photo_CaptureRaw() with a retry loop that discards frames
 *         where the proportion of black pixels exceeds a computed threshold.
 *         After each capture the luma channel of the UYVY buffer is inspected
 *         via count_black_pixels_uyvy(). If the black fraction exceeds
 *         BLACK_REJECT_FRACTION the frame is discarded and a new capture is
 *         attempted, up to `tries` times total.
 *
 *         board_status.images_rejected_black is incremented for every
 *         discarded frame. The slot buffer is left containing the last
 *         captured frame regardless of whether it passed the filter, so the
 *         caller can inspect it if needed.
 *
 *         The sensor must be powered, configured and streaming (CAM_Init()
 *         completed successfully) before calling this function.
 *
 * @param  slot             Raw buffer slot index (0 to RAW_PHOTO_COUNT-1).
 * @param  designator       Global photo sequence number written into the header.
 * @param  opcode           Pointer to OPCODE_SIZE bytes written into the header.
 * @param  tries            Maximum number of capture attempts (including the
 *                          first). Must be >= 1.
 * @param  black_fraction   Maximum tolerable fraction of black pixels allowed.
 *
 * @return HAL_OK      if a frame passed the black filter within `tries` attempts,
 *                     or if all `tries` frames were rejected as too black
 *                     (last captured frame remains in the slot buffer).
 *         HAL_ERROR   if Photo_CaptureRaw() returned HAL_ERROR on any attempt.
 *         HAL_TIMEOUT if Photo_CaptureRaw() timed out on any attempt.
 ********************************************************************************/
HAL_StatusTypeDef Photo_CaptureRawBlack(uint8_t   slot,
                                        uint16_t  designator,
                                        uint8_t  *opcode,
                                        uint8_t   tries,
                                        uint8_t   black_fraction);


#endif	/* __PHOTO_H__ */
