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

#define MIN_INTERVAL 						 		(1U) 		// Minutes in interval for STATE_DELAYED_PICTURE (max. waiting is 256*MIN_INTERVAL minutes)


extern uint32_t picture_delay_start;							// Moment the delayed photo instruction was executed
extern uint8_t picture_delay_mins;								// Amount of N-minute intervals to take a delayed photo
extern uint8_t delayed_flag;									// Flag used to transmit scheduled command buffer for CMD_TakeDelayedPicture() only once
extern uint8_t ignore_flag;										// Used to only transmit "Waiting for reset" in debug UART the first time you enter STATE_IGNORE


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

	CMD_DUMP_RAW_ID        			= 0x11,
	CMD_DUMP_COMPRESSED_ID 			= 0x12,

	CMD_ERASE_FRAM_ID			    = 0x88,
	CMD_FORCE_RESET_ID			    = 0x89,

} cmd_id_t;

// Assignment of command returns with macro values
typedef enum {
	CMD_OK = COMMAND_SUCCESS,									// Correct command execution
	CMD_ERROR = COMMAND_ERROR,									// Generic error
	CMD_NOT_FOUND = COMMAND_NOT_FOUND_FAILURE,					// Command not found in table
	CMD_INCORRECT_PARS = COMMAND_INCORRECT_PARAMETER_FAILURE,	// Wrong number of parameters for desired command
	CMD_SCHEDULED = COMMAND_SCHEDULED,							// Command accepted, execution deferred
	CMD_BUFFER_UNOCCUPIED = COMMAND_BUFFER_UNOCCUPIED, 			// Buffer requested unoccupied
	CMD_BUFFER_OOB = COMMAND_BUFFER_OUT_OF_BOUNDS,				// Out of bounds buffer access
	CMD_CAM_BOOT_ERROR = COMMAND_CAM_BOOT_ERROR,				// Camera not responsive on boot
	CMD_CAM_DCMI_ERROR = COMMAND_CAM_DCMI_ERROR,				// Photo could not be captured through DCMI
	CMD_COMPRESS_ERROR = COMMAND_COMPRESS_ERROR,				// Photo could not be compressed
	CMD_FRAM_FULL = COMMAND_FRAM_FULL,							// Compressions not saved because FRAM is full
	CMD_BUFFER_INVALID = COMMAND_BUFFER_INVALID,				// Buffer number does not exist
	CMD_INDEX_FULL = COMMAND_INDEX_FULL							// Trying to save a compression higher than MAX_COMPRESSED_PHOTOS
	// ... add more here
} CMD_ReturnStatus;


/********************************************************************************
 * @brief  Function pointer type for command handlers, used by ExecuteCommand()
 *         to dispatch incoming instructions through a uniform interface.
 *
 * @note   All command handler implementations must match this signature.
 *         Handlers are typically stored in a lookup table indexed by opcode.
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
 * @brief  Captures a raw photo into raw_buffer_1.
 *
 * @note   Currently fills raw_buffer_1->data with a synthetic gradient for
 *         testing. Replace with Camera_Capture() when camera hardware is
 *         available. Increments board_status.photos_taken and marks
 *         raw_buffer_1_occupied.
 *
 * opcode[0] --> buffer number (4 MSb), CAM number (4 lsb)
 * opcode[1] --> Use black filtering? 0 if no, 1 if yes
 * opcode[2] --> photo tries if black filtering enabled. Otherwise unused
 * opcode[3] --> black threshold for filtering if enabled. Otherwise unised
 * opcode[4] --> Unused for CMD_TakePicture
 *
 * Take a single pic with CAM N (0 for A, 1 for B) and save in BUFFER 0 with opcode: 0N 00 00 00 00
 *
 * @return CMD_OK always.
 *********************************************************************************/
CMD_ReturnStatus CMD_TakePicture(uint8_t *opcode);


/********************************************************************************
 * @brief  Schedules a delayed photo capture after N × MIN_INTERVAL minutes.
 *
 * @note   Reads the delay count from opcode[4], records HAL_GetTick() as the
 *         start time, and writes COMMAND_SCHEDULED + the delay count into
 *         tx_buffer[1:2] for the OBC response. State machine transitions to
 *         STATE_DELAYED_PICTURE via the CMD_SCHEDULED return value.
 *
 * opcode[0:3] --> Same as CMD_TakePicture
 * opcode[4] --> picture delay (max delay = 255*MIN_INTERVAL mins. If MIN_INTERVAL = 5, max. delay is 21h
 *
 * @return CMD_SCHEDULED always.
 *********************************************************************************/
CMD_ReturnStatus CMD_TakePictureDelayed(uint8_t *opcode);


/********************************************************************************
 * @brief  Serializes board_status into tx_buffer[1..] for transmission.
 *
 * @note   Updates board_status.uptime_ms from HAL_GetTick() before copying.
 *         A compile-time static assert verifies that sizeof(board_status_t)
 *         fits within tx_buffer (AIRMAC_SIZE - 1 bytes available).
 *
 * @param  opcode   Unused.
 *
 * @return CMD_OK always.
 *********************************************************************************/
CMD_ReturnStatus CMD_GetStatus(uint8_t *opcode);


/********************************************************************************
 * @brief  Dumpsraw  photo to UART4 (debug).
 *
 * @note   Selects the source buffer from opcode[0] (1, 2, or 3).
 *
 * @param  opcode   opcode[0]: buffer selector.
 *
 * @return CMD_OK on success
 *********************************************************************************/
CMD_ReturnStatus CMD_DumpRaw(uint8_t *opcode);

/********************************************************************************
 * @brief  Dumps compressed photo to UART4 (debug).
 *
 * @note   Dumps compressed SRAM buffer to UART4 (debug)
 *
 * @param  opcode   unused
 * @return CMD_OK on success
 *********************************************************************************/
CMD_ReturnStatus CMD_DumpCompressed(uint8_t *opcode);

/********************************************************************************
 * @brief  Changes parameter configs for both CAMs
 *
 * @note   Selects the source buffer from opcode[0] (1, 2, or 3). Calculates
 *         the byte offset as chunk_index × CHUNK_SIZE, where chunk_index is
 *         the big-endian 16-bit value in opcode[1:2]. Returns
 *         CMD_BUFFER_UNOCCUPIED if the requested buffer is empty or the
 *         buffer choice is invalid, CMD_BUFFER_OOB if the offset exceeds the
 *         total frame size.
 *
 * @param  opcode   opcode[0]: parameter selection
 *                  opcode[1]: MSB of 16b write value
 *                  opcode[2]: LSB of 16b write value
 *
 * @returns CMD_OK
 *********************************************************************************/
CMD_ReturnStatus CMD_ChangeCamParams(uint8_t *opcode);

/********************************************************************************
 * @brief  Compresses raw photo in a given buffer and saves it in SRAM + FRAM
 *
 * @param  opcode   opcode[0]: raw buffer selected
 *                  opcode[1]: Quality of compression: 1, 2 or 3 (from worse to best)
 *
 * @returns CMD_OK or CMD_COMPRESS_ERROR
 *********************************************************************************/
CMD_ReturnStatus CMD_CompressRawPhoto(uint8_t *opcode);


/********************************************************************************
 * @brief  Flags FRAM to be erased on next boot
 *
 * @param  opcode  unused
 *
 * @returns CMD_OK
 *********************************************************************************/
CMD_ReturnStatus CMD_EraseFRAM(uint8_t *opcode);

/********************************************************************************
 * @brief  Performs software reset of USS
 *
 * @param  opcode  unused
 *
 * @returns none
 *********************************************************************************/
CMD_ReturnStatus CMD_ForceReset(uint8_t *opcode);

/********************************************************************************
 * @brief  Sends a Frame in raw buffer back to GS. Will always send 117 bytes
 * 		   starting from address provided.
 * 		   Frame 1 is opcode 00 00 00 00
 * 		   Frame 2 is opcode 00 00 00 75
 * 		   Frame 3 is opcode 00 00 00 EA
 * 		   ...
 *
 * @param  opcode   opcode[0]: raw buffer selected
 *                  opcode[1:4]: start address
 *
 * @returns CMD_OK if success
 *********************************************************************************/
CMD_ReturnStatus CMD_SendRawFrame(uint8_t *opcode);

/********************************************************************************
 * @brief  Sends a compressed Frame in FRAM back to GS. Will always send 117 bytes
 * 		   starting from address provided.
 * 		   Frame 1 is opcode 00 00 00 00
 * 		   Frame 2 is opcode 00 00 00 75
 * 		   Frame 3 is opcode 00 00 00 EA
 * 		   ...
 *
 * @param  opcode   opcode[0]: compression selected in FRAM
 *                  opcode[1:4]: start address
 *
 * @returns none		TODO: Document this
 *********************************************************************************/
CMD_ReturnStatus CMD_SendCompFrame(uint8_t *opcode);

/********************************************************************************
 * @brief  Sends a raw buffer header with metadata
 * 		   ...
 *
 * @param  opcode   opcode[0]: raw buffer selected
 *
 * @returns CMD_OK if success
 *********************************************************************************/
CMD_ReturnStatus CMD_SendRawHeader(uint8_t *opcode);


/********************************************************************************
 * @brief  Sends a compressed header with metadata
 * 		   ...
 *
 * @param  opcode   opcode[0]: compressed photo selected in compression table
 *
 * @returns CMD_OK if success
 *********************************************************************************/
CMD_ReturnStatus CMD_SendCompHeader(uint8_t *opcode);


// Command Table definition lives in command.c — add new entries there
extern const command_t command_table[];

// variable that stores number of available commands
extern const uint16_t COMMAND_COUNT;

#endif
