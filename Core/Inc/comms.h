#ifndef __COMMS_H__
#define __COMMS_H__

#include "command.h"
#include <stdint.h>


#define AIRMAC_SIZE 						(119U)		// Max. AIRMAC frame size
#define USS_ID								(165U) 		// RS-485 board ID for UNSAM SpaceSnap

#define LOG_UART_TIMEOUT					(50U)		// Debug UART timeout in ms

#define IGNORE_TIMEOUT_MAX					(20000U) 	// max timeout for LS-02 reset in ms
#define RESET_DEBOUNCE_TIME					(10U) 		// debounce time to avoid GPIO noise in ms

#define HEADER_SIZE							(8U)		// Header common to all command returns (some ignore this to maximize download size. Example: CMD_SendRawFrame()
#define DATA_HEADER_SIZE					(2U)		// This is the header size used for commands that download data and ignore HEADER_SIZE


#define RX_HEADER_SIZE						(1U)		// Rx header previous to main instruction to execute

extern volatile uint8_t rx_buffer[AIRMAC_SIZE+1]; 		// EnduroSat RS-485 incoming buffer
extern uint8_t tx_buffer[AIRMAC_SIZE];					// EnduroSat RS-485 outgoing buffer
extern uint8_t instr_number;							// Current instruction number
extern uint8_t instr_opcode[OPCODE_SIZE];				// Current instruction opcode

extern volatile uint8_t rx_flag;						// Flag for new incoming message
extern volatile uint16_t rx_size;						// size of incoming message through RS-485


// State machine states for program flow
typedef enum {
	STATE_IDLE = 0,					// IDLE           Listening
	STATE_IGNORE,					// IGNORE         LS-02 listening on RS-485 bus
	STATE_EXECUTE_COMMAND,			// EXECUTE        Execute command
	STATE_TRANSMIT_RESPONSE,		// TRANSMIT       Transmit tx_buffer on RS-485 bus
	STATE_DELAYED_PICTURE			// DELAYED PICTURE Set to take a picture after timeout
} app_state_t;


/********************************************************************************
 * @brief  Transmits a timestamped message over the debug UART (huart4).
 *
 * @note   Prepends a "[HHHHH:MM:SS] " timestamp string computed from
 *         HAL_GetTick() before every message, where hours are zero-padded
 *         to 5 digits to support long-duration mission uptime. Transmission
 *         is blocking with LOG_UART_TIMEOUT ms timeout per call.
 *         Uses strlen() to determine message length -- the input string must
 *         be null-terminated. Updates the global timestamp, total_seconds,
 *         hours, minutes, seconds, and timestamp_string variables as a side
 *         effect.
 *
 * @param  message Null-terminated string to transmit.
 ********************************************************************************/
void Log(char *message);


/********************************************************************************
 * @brief  Zeroes all bytes of tx_buffer.
 *
 * @note   Must be called after every transmission to prevent stale data from
 *         leaking into subsequent frames. Called automatically by
 *         TransmitBufferUART() and TransmitBufferRS485() after sending.
 ********************************************************************************/
void ClearTxBuffer(void);


/********************************************************************************
 * @brief  Transmits tx_buffer over UART1 (OBC interface) and clears the buffer.
 *
 * @note   Writes RESPONSE_INIT_BYTE into tx_buffer[0] before sending so the
 *         OBC can identify USS response frames. Transmits exactly AIRMAC_SIZE
 *         bytes with a 200ms timeout, then calls ClearTxBuffer().
 ********************************************************************************/
void TransmitBufferUART(void);


/********************************************************************************
 * @brief  Transmits tx_buffer over UART1 on the RS-485 bus and clears the
 *         buffer.
 *
 * @note   Writes RESPONSE_INIT_BYTE into tx_buffer[0] before sending.
 *         Asserts DE (GPIO PB1 high) and deasserts RE (GPIO PB0 high) on the
 *         RS-485 transceiver before transmission, then deasserts DE and
 *         reasserts RE after transmission to return the transceiver to
 *         receive mode. Transmits current_command_pointer->return_size +
 *         HEADER_SIZE bytes with a 200ms timeout. Calls ClearTxBuffer() and
 *         ResetLS02() after transmission.
 ********************************************************************************/
void TransmitBufferRS485(void);


/********************************************************************************
 * @brief  Pulses the LS-02 RS-485 reset line to re-enable the module.
 *
 * @note   Drives PC9 high for 10ms via HAL_Delay(), then pulls it low.
 *         Called automatically by TransmitBufferRS485() after each
 *         transmission to restore the LS-02 to its listening state.
 *
 *         TODO: Behavior unverified, to test on integration with LS-02 hardware
 ********************************************************************************/
void ResetLS02(void);


/********************************************************************************
 * @brief  Parses rx_buffer to extract the instruction number and opcode, and
 *         validates the frame length against the command's opcode requirement.
 *
 * @note   Reads rx_buffer[RX_HEADER_SIZE] as the instruction number and
 *         looks it up in the command table. If the command takes an opcode,
 *         copies the opcode bytes from rx_buffer into instr_opcode[]; if it
 *         takes no opcode, instr_opcode[] is zeroed. Rejects frames whose
 *         rx_size does not match the expected length for the command's opcode
 *         requirement, writing COMMAND_INCORRECT_PARAMETER_FAILURE into
 *         tx_buffer[1] and returning CMD_INCORRECT_PARS.
 *
 *         CMD_GET_STATUS_ID is explicitly excluded from updating
 *         board_status.last_instruction and board_status.last_cmd_status,
 *         to avoid a status poll overwriting the last meaningful command
 *         record.
 *
 * @return CMD_OK if the frame is valid and the command was found.
 *         CMD_NOT_FOUND if the instruction number is not in the command table.
 *         CMD_INCORRECT_PARS if rx_size does not match the expected frame
 *         length for the command.
 ********************************************************************************/
CMD_ReturnStatus LoadInstructionBuffer(void);


/********************************************************************************
 * @brief  Top-level handler for frames received on the RS-485 bus.
 *
 * @note   Clears rx_flag on entry. Checks rx_buffer[0] against USS_ID and
 *         current state: if the frame is addressed to USS and the board is not
 *         in STATE_IGNORE, calls LoadInstructionBuffer() and transitions to
 *         STATE_EXECUTE_COMMAND on success or STATE_TRANSMIT_RESPONSE on
 *         error. Any pending delayed photo (delayed_flag == 1) is cancelled
 *         (cleared) on a valid USS frame.
 *
 *         Frames not addressed to USS call DisableRS485() and transition to
 *         fallback_state. If a delayed photo was pending on a non-USS frame,
 *         it is logged but delayed_flag is NOT cleared -- the delayed photo
 *         remains scheduled.
 *
 * @param  fallback_state State to enter when the received frame is not
 *                        addressed to USS (e.g. STATE_IDLE or
 *                        STATE_DELAYED_PICTURE).
 ********************************************************************************/
void HandleIncomingCommand(app_state_t fallback_state);


/********************************************************************************
 * @brief  Puts the RS-485 transceiver into high-impedance mode to release
 *         the bus, and aborts UART1 reception.
 *
 * @note   Deasserts DE (PB1 low) and deasserts RE (PB0 high) to disable both
 *         the driver and receiver on the RS-485 transceiver, placing it in
 *         high-impedance mode. Calls HAL_UART_AbortReceive() on huart1 to
 *         stop any ongoing DMA/interrupt-driven reception.
 ********************************************************************************/
void DisableRS485(void);


/********************************************************************************
 * @brief  Puts the RS-485 transceiver into listen (receive) mode and
 *         re-arms UART1 for interrupt-driven reception.
 *
 * @note   Asserts RE (PB0 low) to enable the RS-485 receiver and deasserts
 *         DE (PB1 low) to disable the driver. Re-arms UART1 reception via
 *         HAL_UARTEx_ReceiveToIdle_IT() into rx_buffer with a maximum of
 *         AIRMAC_SIZE + 1 bytes, triggered on line idle detection.
 ********************************************************************************/
void EnableListenRS485(void);


/********************************************************************************
 * @brief  Polls the USS RS-485 reset GPIO (PA8) and returns its current state.
 *
 * @note   TODO: This function does not currently work as expected. The STM32
 *         is unable to read PA8 as HIGH when driven externally. Root cause
 *         not yet identified -- possible GPIO configuration issue (input mode,
 *         pull-up/pull-down, or pin conflict). Do not rely on this function
 *         until the hardware bring-up issue is resolved.
 *
 * @return 0 if PA8 is low.
 *         1 if PA8 is high.
 ********************************************************************************/
int PollUSSReset(void);


/********************************************************************************
 * @brief  Logs debug information for a CMD_SendRawFrame chunk over UART4.
 *
 * @note   Prints slot, offset (decimal and hex), frame_size, chunk_size,
 *         remaining bytes before this chunk, and whether this is the final
 *         chunk (including zero-pad byte count if so). Follows with a full
 *         hex dump of the tx_buffer payload region (AIRMAC_SIZE - HEADER_SIZE
 *         bytes), formatted as 16 bytes per row with address offsets.
 *         Called internally by CMD_SendRawFrame() after the payload is
 *         populated and zero-padded.
 *
 * @param  slot        Raw buffer slot index.
 * @param  offset      Byte offset from the start of raw photo data[].
 * @param  frame_size  Total size of the raw photo data[] region in bytes.
 * @param  chunk_size  Number of meaningful payload bytes in this chunk.
 * @param  remaining   Bytes remaining in the frame before this chunk.
 ********************************************************************************/
void LogRawFrameDebug(uint8_t slot, uint32_t offset, uint32_t frame_size,
                       uint32_t chunk_size, uint32_t remaining);


/********************************************************************************
 * @brief  Logs debug information for a CMD_SendRawHeader response over UART4.
 *
 * @note   Prints slot, designator, opcode array (each entry as 4 hex digits),
 *         reconstructed 32-bit timestamp and black_pixels (from their
 *         respective MSB/LSB fields), and header_size. Follows with a hex
 *         dump of the tx_buffer header payload region (header_size bytes),
 *         formatted as 16 bytes per row with address offsets. Called
 *         internally by CMD_SendRawHeader() after the header is copied into
 *         tx_buffer.
 *
 * @param  slot        Raw buffer slot index.
 * @param  raw_buffer  Pointer to the raw_photo_t buffer whose header fields
 *                     are decoded and printed.
 * @param  header_size Size of the raw_photo_t header in bytes (everything
 *                     before data[]).
 ********************************************************************************/
void LogRawHeaderDebug(uint8_t slot, volatile raw_photo_t *raw_buffer, uint32_t header_size);


/********************************************************************************
 * @brief  Logs decoded debug information for a CMD_SendCompHeader response
 *         over UART4.
 *
 * @note   Decodes all compressed_photo_t header fields directly from
 *         tx_buffer[HEADER_SIZE..] by casting to compressed_photo_t*, matching
 *         the layout written by ReadBufferFRAM() in CMD_SendCompHeader().
 *         Prints index, designator, opcode array (each entry as 4 hex digits),
 *         quality, reconstructed 32-bit compression_size, timestamp, and
 *         black_pixels (from their respective MSB/LSB fields), plus the FRAM
 *         source address and header_size. Called internally by
 *         CMD_SendCompHeader() after the header has been read from FRAM into
 *         tx_buffer.
 *
 * @param  index        Compression table index of the entry being sent.
 * @param  fram_address FRAM address from which the header was read.
 * @param  header_size  Size of the compressed_photo_t header in bytes
 *                      (everything before data[]).
 ********************************************************************************/
void LogCompHeaderDebug(uint8_t index, uint32_t fram_address, uint32_t header_size);


/********************************************************************************
 * @brief  Logs debug information for a CMD_SendCompFrame chunk over UART4.
 *
 * @note   Prints index, offset (decimal and hex), header_size, total_size,
 *         jpeg_size, jpeg_start FRAM address, chunk_size, remaining bytes
 *         before this chunk, and whether this is the final chunk (including
 *         zero-pad byte count if so). Follows with a full hex dump of the
 *         tx_buffer payload region (AIRMAC_SIZE - HEADER_SIZE bytes),
 *         formatted as 16 bytes per row with address offsets. Called
 *         internally by CMD_SendCompFrame() after the JPEG chunk has been
 *         read from FRAM into tx_buffer and zero-padded.
 *
 * @param  index        Compression table index of the entry being sent.
 * @param  offset       Byte offset from the start of the JPEG data region.
 * @param  header_size  Size of the compressed_photo_t header in bytes.
 * @param  total_size   Total size of the compression entry in FRAM (header
 *                      + JPEG data) in bytes.
 * @param  jpeg_size    Size of the JPEG data region alone in bytes.
 * @param  jpeg_start   FRAM address of the first byte of JPEG data.
 * @param  chunk_size   Number of meaningful payload bytes in this chunk.
 * @param  remaining    Bytes remaining in the JPEG data before this chunk.
 ********************************************************************************/
void LogCompFrameDebug(uint8_t index, uint32_t offset, uint32_t header_size, uint32_t total_size,
                        uint32_t jpeg_size, uint32_t jpeg_start, uint32_t chunk_size, uint32_t remaining);


/********************************************************************************
 * @brief  Populates tx_buffer with the last instruction number and opcode as
 *         an echo for the ground station to confirm which command is being
 *         acknowledged.
 *
 * @note   Writes board_status.last_instruction into tx_buffer[2] and
 *         opcode[0..4] into tx_buffer[3..7]. Should be called at the end of
 *         every command handler except frame dump commands (CMD_SendRawFrame,
 *         CMD_SendCompFrame), where maximizing payload bytes takes priority.
 *
 * @param  opcode Pointer to the 5-byte opcode array of the current command.
 ********************************************************************************/
void CMD_PopulateEcho(uint8_t *opcode);

#endif
