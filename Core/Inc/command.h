/**
  ******************************************************************************
  * @file           : command.h
  * @brief          : Command dispatch interface — opcodes, table, and shared state
  ******************************************************************************
  * @author         : Lucas Finazzi <lfinazzi@unsam.edu.ar> (2026)
  *
  ******************************************************************************
  */
#ifndef __COMMAND_H__
#define __COMMAND_H__

#include <stdint.h>

#define HAS_OPCODE 									(1U) 		// Command has opcode
#define NO_OPCODE 									(0U) 		// Command has no opcode
#define OPCODE_SIZE									(5U) 		// Bytes in command opcode

#define RESPONSE_INIT_BYTE							(171U)		// Start Byte for USS responses

#define COMMAND_SUCCESS								(44U) 		// Command success
#define COMMAND_SCHEDULED							(45U)		// Command scheduled successfully

#define COMMAND_ERROR								(73U) 		// Generic error failure
#define COMMAND_NOT_FOUND_FAILURE					(74U) 		// Command not found failure
#define COMMAND_INCORRECT_PARAMETER_FAILURE			(75U)		// Too many or too few parameters in instruction requested
#define COMMAND_BUFFER_UNOCCUPIED					(76U)		// No photo saved in buffer
#define COMMAND_BUFFER_OUT_OF_BOUNDS				(77U)		// Out of bounds access requested in buffer
#define COMMAND_CAM_BOOT_ERROR						(78U)		// Camera not responsive on boot
#define COMMAND_CAM_DCMI_ERROR						(79U)		// DCMI capture error
#define COMMAND_COMPRESS_ERROR						(80U)		// JPEG compression error of raw buffer
#define COMMAND_FRAM_FULL							(81U)		// Compressions not saved because FRAM is full
#define COMMAND_BUFFER_INVALID						(82U)		// Buffer does not exist
#define COMMAND_INDEX_FULL							(83U)		// Trying to save a compression higher than MAX_COMPRESSED_PHOTOS
#define COMMAND_CONFIRM_FAILED						(84U)		// Failed to confirm opcode to execute dangerous instruction
#define COMMAND_PARAM_INVALID						(85U)		// Invalid value for parameter in command

#define MIN_INTERVAL 						 		(1U) 		// Minutes in interval for STATE_DELAYED_PICTURE (max. waiting is 256*MIN_INTERVAL minutes)


// Definition of command IDs, starting at 0x33 are mission commands, starting at 0x11 are debug commands, 0x88 start of danger zone command
typedef enum {
	CMD_TAKE_PICTURE_ID         	= 0x33,
	CMD_TAKE_PICTURE_DELAYED_ID,   // 0x34
	CMD_CHANGE_CAM_PARAMS_ID,	   // 0x35
	CMD_COMPRESS_PHOTO_ID,		   // 0x36
	CMD_SEND_RAW_FRAME_ID,		   // 0x37
	CMD_SEND_COMP_FRAME_ID,  	   // 0x38
	CMD_SEND_RAW_HEADER_ID,		   // 0x39
	CMD_SEND_COMP_HEADER_ID,	   // 0x3A
	CMD_GET_STATUS_ID,             // 0x3B
	CMD_CHANGE_BURST_PARAMS_ID,    // 0x3C
	CMD_TAKE_PICTURE_BURST_ID,     // 0x3D

	CMD_DUMP_RAW_ID        			= 0x11,
	CMD_DUMP_COMPRESSED_ID 			= 0x12,
	CMD_DUMP_SRAM_BIN_ID        	= 0x13,
	CMD_DUMP_FRAM_BIN_ID 			= 0x14,

	/* DANGER ZONE */
	CMD_ERASE_FRAM_ID			    = 0x88,		// confirmation: 0a 0f 0a 0f 0a
	CMD_FORCE_RESET_ID			    = 0x89,
	CMD_ERASE_COMP_ID				= 0x90,		// confirmation: ba bf ba bf ba
	CMD_BACKUP_FIRMWARE_ID			= 0x91,		// confirmation: b4 c4 b4 c4 b4

} cmd_id_t;

// Assignment of command returns with macro values
typedef enum {
	CMD_OK 					= COMMAND_SUCCESS,						// Correct command execution
	CMD_ERROR 				= COMMAND_ERROR,						// Generic error
	CMD_NOT_FOUND 			= COMMAND_NOT_FOUND_FAILURE,			// Command not found in table
	CMD_INCORRECT_PARS 		= COMMAND_INCORRECT_PARAMETER_FAILURE,	// Wrong number of parameters for desired command
	CMD_SCHEDULED 			= COMMAND_SCHEDULED,					// Command accepted, execution deferred
	CMD_BUFFER_UNOCCUPIED 	= COMMAND_BUFFER_UNOCCUPIED, 			// Buffer requested unoccupied
	CMD_BUFFER_OOB 			= COMMAND_BUFFER_OUT_OF_BOUNDS,			// Out of bounds buffer access
	CMD_CAM_BOOT_ERROR 		= COMMAND_CAM_BOOT_ERROR,				// Camera not responsive on boot
	CMD_CAM_DCMI_ERROR 		= COMMAND_CAM_DCMI_ERROR,				// Photo could not be captured through DCMI
	CMD_COMPRESS_ERROR 		= COMMAND_COMPRESS_ERROR,				// Photo could not be compressed
	CMD_FRAM_FULL 			= COMMAND_FRAM_FULL,					// Compressions not saved because FRAM is full
	CMD_BUFFER_INVALID 		= COMMAND_BUFFER_INVALID,				// Buffer number does not exist
	CMD_INDEX_FULL 			= COMMAND_INDEX_FULL,					// Trying to save a compression higher than MAX_COMPRESSED_PHOTOS
	CMD_CONFIRM_FAILED 		= COMMAND_CONFIRM_FAILED,				// Failed to confirm opcode needed for instruction execution
	CMD_PARAM_INVALID 		= COMMAND_PARAM_INVALID					// Invalid value for parameter in instruction
	// ... add more here
} CMD_ReturnStatus;


/********************************************************************************
 * @brief  Function pointer type for command handlers, used by ExecuteCommand()
 *         to dispatch incoming instructions through a uniform interface.
 *
 * @note   All command handler implementations must match this signature.
 *         Handlers are stored in a lookup table indexed by opcode.
 *
 * @param  opcode    Pointer to the received opcode byte(s) for the instruction
 *                   to be executed.
 *
 * @return CMD_ReturnStatus indicating the outcome of the command execution.
 *********************************************************************************/
typedef CMD_ReturnStatus (*command_handler_t)(uint8_t*);


// Command structure definition
typedef struct {
    const char *name;                 // Human-readable name
    uint8_t instruction_number;       // Numeric command ID
    command_handler_t handler;        // Function pointer to execute
    int takes_opcode;		      	  // Indicates if instruction takes opcode
    uint8_t return_size;			  // Size in bytes for return
} command_t;


// Pointer to current command/instruction
extern const command_t* current_command_pointer;

// return used to identify status return in CMD calls
extern CMD_ReturnStatus cmd_ret;


/********************************************************************************
 * @brief  Dispatches a command by invoking its handler via function pointer,
 *         then writes the appropriate status byte into tx_buffer[1] via
 *         ReturnCode(). Updates board_status with the last instruction and
 *         opcode, except when the command is CMD_GET_STATUS_ID.
 *
 * @param  command   Pointer to the command_t entry to execute. If NULL, sets
 *                   tx_buffer[1] to COMMAND_NOT_FOUND_FAILURE and returns
 *                   CMD_NOT_FOUND without invoking any handler.
 * @param  opcode    Pointer to the opcode byte array parsed from the incoming
 *                   frame. Passed unchanged to the handler.
 *
 * @return CMD_ReturnStatus returned by the command handler, or CMD_NOT_FOUND
 *         if command is NULL.
 *********************************************************************************/
CMD_ReturnStatus ExecuteCommand(const command_t *command, uint8_t *opcode);


/********************************************************************************
 * @brief  Looks up a command in command_table by its instruction number.
 *
 * @param  instruction_number   Numeric command ID received in the RS-485 frame.
 *
 * @return Pointer to the matching command_t entry, or NULL if no entry with
 *         that instruction_number exists in the table.
 *********************************************************************************/
const command_t* GetCommand(uint8_t instruction_number);


/********************************************************************************
 * @brief  Delay helper function.
 *
 * @note   Kicks IWDG every 1000 ms to avoid reset. WARNING: This function is
 * 		   blocking, so avoid using high values of seconds
 *********************************************************************************/
void delay_kick_wdg(uint16_t seconds);


/********************************************************************************
 * @brief  Captures a single raw photo using the selected camera and stores it
 *         in the selected raw photo buffer.
 *
 * @note   Activates and initializes the selected camera (A or B), captures a
 *         raw frame via Photo_CaptureRaw(), then deactivates the camera.
 *         Increments board_status.photos_taken and marks the corresponding
 *         raw_buffer_N_occupied flag on success.
 *
 *         If filter_flag is set, the function should count black pixels in the
 *         captured image and, if the count number exceeds black_fraction, retake the
 *         photo, up to a maximum of "tries" attempts, before giving up.
 *
 * @note   Calls CMD_PopulateEcho() to write the instruction number and opcode
 *         into tx_buffer for ground station acknowledgement.
 *
 * @param  opcode Pointer to a 5-byte opcode array:
 *                opcode[0] --> buffer number (4 MSb), CAM number (4 LSb)
 *                opcode[1] --> lower nibble: black filtering (0 = no, non-zero = yes)
 *                              upper nibble: advanced mode (0 = basic AE, non-zero = manual exposure/gain)
 *                opcode[2] --> photo tries if black filtering enabled, otherwise unused
 *                opcode[3] --> black fraction for filtering if enabled, otherwise unused, Values possible are 0-200 (each is 0.5% of total pixels)
 *                opcode[4] --> unused for CMD_TakePicture
 *
 *                Example: take a single pic with CAM B and save in BUFFER 0
 *                with opcode: 01 00 00 00 00
 *
 * @return CMD_OK on success.
 *         CMD_BUFFER_INVALID if buffer_number is out of range.
 *         CMD_CAM_BOOT_ERROR if the selected camera fails to initialize, or if
 *         cam_number is invalid (not 0 or 1).
 *         CMD_CAM_DCMI_ERROR if Photo_CaptureRaw() fails.
 *         CMD_PARAM_INVALID if black fraction is higher than allowed value (200).
 ********************************************************************************/
CMD_ReturnStatus CMD_TakePicture(uint8_t *opcode);


/********************************************************************************
 * @brief  Captures many raw photos using the selected camera and stores it
 *         in appropriate buffers
 *
 * @note   Activates and initializes the selected camera (A or B), captures many frames
 * 		   via Photo_CaptureRaw(), then deactivates the camera.
 *         Increments board_status.photos_taken and marks the corresponding
 *         raw buffers occupied on success.
 *
 *         If filter_flag is set, the function should count black pixels in the
 *         captured images and, if the count number exceeds black_fraction, retake the
 *         photo, up to a maximum of "tries" attempts, before giving up.
 *
 * @note   Calls CMD_PopulateEcho() to write the instruction number and opcode
 *         into tx_buffer for ground station acknowledgement.
 *
 * @note   This function gets some parameters from board_status.delayed_params,
 *         which include: number of photos to take, time between them, if
 *         photos should be compressed and stored in FRAM and the quality of
 *         that compression.
 *
 * @param  opcode Pointer to a 5-byte opcode array:
 *                opcode[0] --> buffer number (4 MSb), CAM number (4 LSb)
 *                opcode[1] --> lower nibble: black filtering (0 = no, non-zero = yes)
 *                              upper nibble: advanced mode (0 = basic AE, non-zero = manual exposure/gain)
 *                opcode[2] --> photo tries if black filtering enabled, otherwise unused
 *                opcode[3] --> black fraction for filtering if enabled, otherwise unused, Values possible are 0-200 (each is 0.5% of total pixels)
 *                opcode[4] --> unused for CMD_TakePictureBurst
 *
 *
 * @return CMD_OK on success.
 *         CMD_BUFFER_INVALID if buffer_number is out of range.
 *         CMD_CAM_BOOT_ERROR if the selected camera fails to initialize, or if
 *         cam_number is invalid (not 0 or 1).
 *         CMD_CAM_DCMI_ERROR if Photo_CaptureRaw() fails.
 *         CMD_PARAM_INVALID if black fraction is higher than allowed value (200)
 *         or buffers are not available to save board_status.delayed_params.num_photos
 *         captures.
 ********************************************************************************/
CMD_ReturnStatus CMD_TakePictureBurst(uint8_t *opcode);


/********************************************************************************
 * @brief  Schedules delayed photo captures after N x MIN_INTERVAL minutes.
 *
 * @note   Reads the delay count from opcode[4], records the current
 *         board timestamp (uptime_total + uptime_session/1000) as
 *         delayed_start, and writes COMMAND_SCHEDULED into tx_buffer[1]
 *         for the OBC response. State machine transitions to
 *         STATE_DELAYED_PICTURE via the CMD_SCHEDULED return value.
 *
 * @note   Calls CMD_PopulateEcho() to write the instruction number and opcode
 *         into tx_buffer for ground station acknowledgement.
 *
 * @note   This command schedules delayed photos (up to 5, due to raw buffer
 * 		   limitations), separated by configurable time and also has the option
 * 		   to compress and save this pictures in FRAM automatically. The settings
 * 		   for these burst captures are found in board_status.delayed_params.
 *
 *
 * @param  opcode Pointer to a 5-byte opcode array:
 *                opcode[0:3] --> same as CMD_TakePicture
 *                opcode[4]   --> picture delay, in units of MIN_INTERVAL minutes
 *                                (max delay = 255 x MIN_INTERVAL mins; if
 *                                MIN_INTERVAL = 5, max delay is 21h)
 *
 * @return CMD_SCHEDULED always.
 ********************************************************************************/
CMD_ReturnStatus CMD_TakePictureDelayed(uint8_t *opcode);


/********************************************************************************
 * @brief  Serializes board_status into tx_buffer[1..] for transmission.
 *
 * @note   Updates board_status.uptime_session from HAL_GetTick() before
 *         copying. A compile-time static assert verifies that
 *         sizeof(board_status_t) + sizeof(fw_backup_info_t) fits within
 *         tx_buffer (AIRMAC_SIZE - DATA_HEADER_SIZE bytes available). Also
 *         calls LogBoardStatusFull() to print a full field-by-field breakdown of 
 *         board_status over UART4 (debug) for human viewing.
 *
 * @param  opcode Unused.
 *
 * @return CMD_OK always.
 ********************************************************************************/
CMD_ReturnStatus CMD_GetStatus(uint8_t *opcode);


/********************************************************************************
 * @brief  Dumps a raw photo buffer's header fields and pixel data to UART4
 *         (debug).
 *
 * @note   Selects the source buffer from opcode[0]. Prints the opcode array
 *         and reconstructed 32-bit timestamp, then calls DumpRawBuffer() to
 *         hex-dump the full pixel data (L * H * sizeof(uint16_t) bytes) over
 *         UART4. This can take a while given the size of a full raw frame.
 *
 * @note   Calls CMD_PopulateEcho() to write the instruction number and opcode
 *         into tx_buffer for ground station acknowledgement.
 *
 *         NOTE: opcode[] is declared as uint16_t[OPCODE_SIZE] in raw_photo_t,
 *         but is currently printed with %02X (byte-width) format specifiers
 *		   on purpose.
 *
 * @param  opcode opcode[0]: raw buffer number to dump (0 to RAW_PHOTO_COUNT-1).
 *
 * @return CMD_OK on success.
 *         CMD_BUFFER_INVALID if slot is out of range.
 ********************************************************************************/
CMD_ReturnStatus CMD_DumpRaw(uint8_t *opcode);


/********************************************************************************
 * @brief  Dumps the compressed photo buffer (header fields and JPEG data) to
 *         UART4 (debug).
 *
 * @note   Only one compressed SRAM buffer exists (index 0), so no buffer
 *         selector opcode is needed. Checks board_status.compression_buffer_occupied
 *         before proceeding, since the SRAM buffer may not hold a valid
 *         compression even if one was performed in a previous boot session
 *         (SRAM contents do not persist across resets). Prints the opcode
 *         array, reconstructed 32-bit timestamp, and quality, then calls
 *         DumpCompressedBuffer() to hex-dump the compressed JPEG data over
 *         UART4.
 *
 * @note   Calls CMD_PopulateEcho() to write the instruction number and opcode
 *         into tx_buffer for ground station acknowledgement.
 *
 *         NOTE: opcode[] is declared as uint16_t[OPCODE_SIZE] in raw_photo_t,
 *         but is currently printed with %02X (byte-width) format specifiers
 *		   on purpose.
 *
 * @param  opcode Unused.
 *
 * @return CMD_OK on success.
 *         CMD_BUFFER_INVALID if no compression currently exists in the SRAM
 *         buffer (board_status.compression_buffer_occupied == 0).
 ********************************************************************************/
CMD_ReturnStatus CMD_DumpCompressed(uint8_t *opcode);


/********************************************************************************
 * @brief  Changes a configurable camera parameter shared by both CAMs.
 *
 * @note   Currently (black_threshold, gain (analog and digital,
 * 		   coarse and fine exposure) are implemented.
 *
 *         Add other parameters as needed.
 *
 *         Selects which parameter to modify via opcode[0], and writes the
 *         16-bit value reconstructed from opcode[1:2] (big-endian) into the
 *         corresponding field in cam_params. Idx 0 is reserved to put all
 *         parameters to default values.
 *
 * @note   Calls CMD_PopulateEcho() to write the instruction number and opcode
 *         into tx_buffer for ground station acknowledgement.
 *
 * @param  opcode opcode[0]: index of the parameter to change (0 = reset all to defaults)
 *                opcode[1]: value MSB
 *                opcode[2]: value LSB
 *
 * @return CMD_OK if idx matches a known parameter and the value was written.
 *         CMD_ERROR if idx does not match any known parameter.
 ********************************************************************************/
CMD_ReturnStatus CMD_ChangeCamParams(uint8_t *opcode);


/********************************************************************************
 * @brief  Compresses the raw photo in a given buffer and saves the result in
 *         both the SRAM compression buffer and FRAM.
 *
 * @note   Calls CompressRawPhoto() to perform the actual compression into the
 *         single SRAM compressed_photo_t buffer. On success, computes the
 *         header and JPEG sizes, checks that the FRAM region before
 *         FIRMWARE_BACKUP_START has enough space and that the compression
 *         index table is not full, then writes the header and JPEG data to
 *         FRAM at board_status.compression_ptr_address, advancing the
 *         pointer and updating board_status.fram_bytes_left accordingly.
 *         Adds a new entry to compression_table[] and increments
 *         board_status.compression_count, compressions_done, and sets
 *         compression_buffer_occupied.
 *
 * @note   Calls CMD_PopulateEcho() to write the instruction number and opcode
 *         into tx_buffer for ground station acknowledgement.
 *
 * @param  opcode opcode[0]: raw buffer selected
 *                opcode[1]: quality of compression: 1, 2, or 3 (worst to best)
 *
 * @return CMD_OK on success.
 *         CMD_BUFFER_INVALID if buffer is out of range.
 *         CMD_COMPRESS_ERROR if CompressRawPhoto() fails.
 *         CMD_FRAM_FULL if there is not enough remaining FRAM space before
 *         FIRMWARE_BACKUP_START to store the compressed result.
 *         CMD_INDEX_FULL if compression_table[] has reached MAX_COMPRESSED_PHOTOS.
 ********************************************************************************/
CMD_ReturnStatus CMD_CompressRawPhoto(uint8_t *opcode);


/********************************************************************************
 * @brief  Erases FRAM immediately, gated behind a fixed confirmation opcode.
 *
 * @note   Requires opcode to exactly match the confirmation sequence
 *         0A 0F 0A 0F 0A before proceeding, to reduce the risk of accidental
 *         erasure from a corrupted/garbled command. Calls EraseFRAM(), which
 *         performs the erase synchronously before returning.
 *
 * @note   Calls CMD_PopulateEcho() to write the instruction number and opcode
 *         into tx_buffer for ground station acknowledgement.
 *
 * @param  opcode Must exactly equal {0x0A, 0x0F, 0x0A, 0x0F, 0x0A} for the
 *                erase to proceed.
 *
 * @return CMD_OK on success.
 *         CMD_CONFIRM_FAILED if opcode does not match the required
 *         confirmation sequence.
 ********************************************************************************/
CMD_ReturnStatus CMD_EraseFRAM(uint8_t *opcode);


/********************************************************************************
 * @brief  Performs a software reset of the USS via IWDG timeout.
 *
 * @note   Increments board_status.requested_power_downs and persists
 *         board_status to FRAM before resetting, so the counter survives the
 *         reboot. Transmits a COMMAND_SUCCESS response to the ground station
 *         over RS485 before resetting, so the OBC receives an acknowledgement.
 *
 *         Disables all interrupts, then reconfigures the IWDG to its fastest
 *         possible timeout (prescaler 4, reload 0, ~125us) and spins in an
 *         infinite loop until the watchdog fires and resets the MCU.
 *
 *         The return statement (HAL_OK) is unreachable by design -- the
 *         function never returns normally.
 *
 * @note   Calls CMD_PopulateEcho() to write the instruction number and opcode
 *         into tx_buffer for ground station acknowledgement.
 *
 * @param  opcode Unused.
 *
 * @return Never returns. Included for CMD_ReturnStatus signature conformance.
 ********************************************************************************/
CMD_ReturnStatus CMD_ForceReset(uint8_t *opcode);


/********************************************************************************
 * @brief  Sends a chunk of raw photo data from SRAM back to the ground station.
 *         Always sends AIRMAC_SIZE - DATA_HEADER_SIZE bytes, zero-padded on
 *         the final partial chunk.
 *
 *         Frame 1 is opcode 00 00 00 00 00
 *         Frame 2 is opcode 00 00 00 00 75
 *         Frame 3 is opcode 00 00 00 00 EA
 *         ...
 *
 * @note   The offset is a big-endian 32-bit value reconstructed from
 *         opcode[1:4], representing the byte offset from the start of the
 *         raw photo data[] array. The final chunk is zero-padded to fill
 *         the full AIRMAC_SIZE - DATA_HEADER_SIZE payload.
 *
 *         CMD_PopulateEcho() is intentionally NOT called in this function
 *         to maximize the number of payload bytes sent per chunk.
 *
 * @param  opcode opcode[0]:   raw buffer selected (0 to RAW_PHOTO_COUNT-1)
 *                opcode[1:4]: byte offset from start of raw photo data[]
 *                             (big-endian 32-bit value)
 *
 * @return CMD_OK on success.
 *         CMD_BUFFER_INVALID if slot is out of range.
 *         CMD_BUFFER_OOB if offset is >= frame_size.
 ********************************************************************************/
CMD_ReturnStatus CMD_SendRawFrame(uint8_t *opcode);


/********************************************************************************
 * @brief  Sends a chunk of compressed JPEG data from FRAM back to the ground
 *         station. Always sends AIRMAC_SIZE - DATA_HEADER_SIZE bytes,
 *         zero-padded on the final partial chunk.
 *
 *         Frame 1 is opcode 00 00 00 00 00
 *         Frame 2 is opcode 00 00 00 00 75
 *         Frame 3 is opcode 00 00 00 00 EA
 *         ...
 *
 * @note   The offset is a big-endian 32-bit value reconstructed from
 *         opcode[1:4], representing the byte offset from the start of the
 *         JPEG data region (i.e. after the compressed_photo_t header) of the
 *         selected compression entry in FRAM. The FRAM read address is
 *         computed as compression_table[index].fram_address + header_size +
 *         offset. The final chunk is zero-padded to fill the full
 *         AIRMAC_SIZE - DATA_HEADER_SIZE payload.
 *
 *         CMD_PopulateEcho() is intentionally NOT called in this function
 *         to maximize the number of payload bytes sent per chunk.
 *
 * @param  opcode opcode[0]:   compression index in FRAM
 *                             (0 to compression_count - 1)
 *                opcode[1:4]: byte offset from start of JPEG data region
 *                             (big-endian 32-bit value)
 *
 * @return CMD_OK on success.
 *         CMD_BUFFER_INVALID if index >= compression_count or
 *         compression_table[index].valid is not set.
 *         CMD_BUFFER_OOB if offset >= jpeg_size.
 ********************************************************************************/
CMD_ReturnStatus CMD_SendCompFrame(uint8_t *opcode);


/********************************************************************************
 * @brief  Sends the header metadata of a raw photo buffer to the ground
 *         station in a single response (no chunking needed).
 *
 * @note   Copies the raw_photo_t header fields (everything before data[]:
 *         designator, opcode, timestamp, black_pixels) into tx_buffer
 *         starting at tx_buffer[HEADER_SIZE]. The header is fixed-size and
 *         always fits within a single AIRMAC_SIZE response.
 *         Calls PopulateEcho() to write the instruction number and opcode
 *         into tx_buffer for ground station acknowledgement.
 *
 * @param  opcode opcode[0]: raw buffer selected (0 to RAW_PHOTO_COUNT-1)
 *
 * @return CMD_OK on success.
 *         CMD_BUFFER_INVALID if slot is out of range.
 ********************************************************************************/
CMD_ReturnStatus CMD_SendRawHeader(uint8_t *opcode);


/********************************************************************************
 * @brief  Sends the header metadata of a compressed photo entry from FRAM
 *         to the ground station in a single response (no chunking needed).
 *
 * @note   Reads the compressed_photo_t header fields (everything before
 *         data[]: index, designator, opcode, quality, size, timestamp,
 *         black_pixels) from FRAM at compression_table[index].fram_address
 *         into tx_buffer starting at tx_buffer[HEADER_SIZE]. The header is
 *         fixed-size and always fits within a single AIRMAC_SIZE response.
 *         Calls PopulateEcho() to write the instruction number and opcode
 *         into tx_buffer for ground station acknowledgement.
 *
 * @param  opcode opcode[0]: compression index in the compression table
 *                           (0 to compression_count - 1)
 *
 * @return CMD_OK on success.
 *         CMD_BUFFER_INVALID if index >= compression_count or
 *         compression_table[index].valid is not set.
 ********************************************************************************/
CMD_ReturnStatus CMD_SendCompHeader(uint8_t *opcode);


/********************************************************************************
 * @brief  Erases all compressed photo data from FRAM and resets the
 *         compression table and related board_status fields.
 *
 * @note   Requires opcode to exactly match the confirmation sequence
 *         BA BF 0A 0F 0A before proceeding, to reduce the risk of accidental
 *         erasure from a corrupted/garbled command. Calls EraseCompressions(),
 *         which writes 0x00 to every byte from PHOTO_DATA_START to
 *         FIRMWARE_BACKUP_START via FRAM_WriteByte(). This operation takes
 *         approximately 40 seconds. The IWDG is kicked on every byte write
 *         to prevent a watchdog reset during the erase.
 *
 *         After the FRAM erase, resets the in-RAM compression_table[] to
 *         zero, resets board_status.compression_ptr_address to
 *         PHOTO_DATA_START, recalculates board_status.fram_bytes_left, and
 *         clears board_status.compression_count.
 *
 *         Calls PopulateEcho() to write the instruction number and opcode
 *         into tx_buffer for ground station acknowledgement.
 *
 * @param  opcode Must exactly equal {0xBA, 0xBF, 0xBA, 0xBF, 0xBA} for the
 *                erase to proceed.
 *
 * @return CMD_OK on success.
 *         CMD_CONFIRM_FAILED if opcode does not match the required
 *         confirmation sequence.
 ********************************************************************************/
CMD_ReturnStatus CMD_EraseCompressions(uint8_t *opcode);


/********************************************************************************
 * @brief  Dumps the entire SRAM raw photo buffer region as raw binary over
 *         UART4 (debug).
 *
 * @note   Transmits all bytes from RAW_PHOTO_BASE_ADDRESS to END_OF_BUFFERS
 *         as raw binary via HAL_UART_Transmit() in 256-byte blocking chunks
 *         over huart4. Kicks the IWDG after each chunk. This operation
 *         transfers several MB and takes a significant amount of time
 *         depending on UART baud rate -- do not use in time-critical paths.
 *
 *         Calls PopulateEcho() to write the instruction number and opcode
 *         into tx_buffer for ground station acknowledgement.
 *
 *         ---> CMD to capture bytes with python to decode full memory
 *              Takes approximately 1m 20s
 *
 * @param  opcode Unused.
 *
 * @return CMD_OK always.
 ********************************************************************************/
CMD_ReturnStatus CMD_DumpAllSRAM(uint8_t *opcode);


/********************************************************************************
 * @brief  Dumps the entire FRAM address space as raw binary over UART4
 *         (debug).
 *
 * @note   Reads and transmits all bytes from BOARD_STATUS_START (0x00) to
 *         END_OF_FRAM in 256-byte burst reads via ReadBufferFRAM(), which
 *         uses a single SPI READ command with address auto-increment per
 *         chunk (significantly faster than byte-at-a-time reads). Each
 *         256-byte chunk is transmitted via HAL_UART_Transmit() on huart4
 *         then the IWDG is kicked before the next chunk. Includes the full
 *         FRAM address space: board status, compression table, photo data,
 *         and firmware backup regions.
 *
 *         Calls PopulateEcho() to write the instruction number and opcode
 *         into tx_buffer for ground station acknowledgement.
 *
 *         ---> CMD to capture bytes with python to decode full memory
 *              Takes approximately 50s
 *
 * @param  opcode Unused.
 *
 * @return CMD_OK always.
 ********************************************************************************/
CMD_ReturnStatus CMD_DumpAllFRAM(uint8_t *opcode);


/********************************************************************************
 * @brief  Backs up the application firmware image from internal flash to
 *         the FRAM backup region as two redundant copies, then verifies
 *         each copy by reading back and recomputing the CRC32.
 *
 * @note   Requires an exact 5-byte confirmation opcode (0xB4 0xC4 0xB4
 *         0xC4 0xB4) to prevent accidental invocation. Derives the
 *         application image bounds from the linker-exported symbols
 *         _app_flash_start and _app_flash_end. Writes two independent
 *         copies via WriteFirmwareCopy(): copy A at FIRMWARE_IMAGE_A_START
 *         and copy B at FIRMWARE_IMAGE_B_START. Each copy is streamed in
 *         256-byte chunks via SaveFRAM_Unlocked() with a running
 *         zlib-compatible CRC32 (poly 0xEDB88320, init/final XOR
 *         0xFFFFFFFF) matching the bootloader's CRC32_Calculate(). After
 *         each copy the image is read back from FRAM and the CRC is
 *         recomputed to catch SPI/FRAM write faults. Only after both
 *         copies are verified does the function write the fw_backup_info
 *         header (app_size, CRC32, version) to both
 *         FIRMWARE_BACKUP_A_START and FIRMWARE_BACKUP_B_START. IWDG is
 *         refreshed between chunks. Returns CMD_ERROR without updating
 *         either header if app_size is out of range or either readback
 *         CRC mismatches.
 *
 * @param  opcode   Pointer to the 5-byte opcode field from the AirMAC
 *                  frame. Must exactly match the confirmation sequence
 *                  or the command aborts immediately.
 *
 * @retval CMD_ReturnStatus   CMD_OK on success. CMD_CONFIRM_FAILED if
 *                            the confirmation sequence does not match.
 *                            CMD_ERROR if app_size is out of range or
 *                            either FRAM readback CRC does not match the
 *                            computed CRC.
 ********************************************************************************/
CMD_ReturnStatus CMD_BackupFirmware(uint8_t *opcode);


/********************************************************************************
 * @brief  Changes configurable burst parameters for taking many pictures
 * 		   with CMD_TakePictureDelayed().
 *
 * @param  opcode opcode[0]: 0x00 to reset defaults, non-zero to modify burst params
 *                opcode[1]: number of burst photos
 *                opcode[2]: time in seconds between photos
 *                opcode[3]: perform compressions?
 *                opcode[4]: compression quality
 *
 * @return CMD_OK if valid parameters.
 *         CMD_PARAM_INVALID if invalid parameter is desired.
 ********************************************************************************/
CMD_ReturnStatus CMD_ChangeBurstParams(uint8_t *opcode);


// Command Table definition lives in command.c — add new entries there
extern const command_t command_table[];

// variable that stores number of available commands
extern const uint16_t COMMAND_COUNT;

#endif	/* __COMMAND_H__ */
