#ifndef __COMMS_H__
#define __COMMS_H__

#include "command.h"
#include <stdint.h>


#define AIRMAC_SIZE 						(119U)		// Max. AIRMAC frame size
#define USS_ID								(165U) 		// RS-485 board ID for UNSAM SpaceSnap

#define LOG_UART_TIMEOUT					(50U)		// Debug UART timeout in ms

extern volatile uint8_t rx_buffer[AIRMAC_SIZE+1]; 		// EnduroSat RS-485 incoming buffer
extern uint8_t tx_buffer[AIRMAC_SIZE];					// EnduroSat RS-485 outgoing buffer
extern uint8_t instr_number;							// Current instruction number
extern uint8_t instr_opcode[OPCODE_SIZE];				// Current instruction opcode

extern volatile uint8_t rx_flag;						// Flag for new incoming message
extern volatile uint16_t rx_size;						// size of incoming message through RS-485

extern volatile uint8_t uss_comm_reset;					// LS-02 input reset


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
 * @note   Prepends a "[HH:MM:SS] " timestamp string computed from HAL_GetTick()
 *         before every message. Blocking transmission with LOG_UART_TIMEOUT ms
 *         timeout. Updates the global timestamp, hours, minutes, seconds, and
 *         timestamp_string variables as a side effect.
 *
 * @param  message   Null-terminated string to transmit.
 *********************************************************************************/
void Log(char *message);


/********************************************************************************
 * @brief  Zeroes all bytes of tx_buffer.
 *
 * @note   Must be called after every transmission to prevent stale data from
 *         leaking into subsequent frames. Called automatically by
 *         TransmitBufferUART() after sending.
 *********************************************************************************/
void ClearTxBuffer(void);


/********************************************************************************
 * @brief  Transmits tx_buffer over UART1 (OBC interface) and clears the buffer.
 *
 * @note   Writes RESPONSE_INIT_BYTE into tx_buffer[0] before sending so the
 *         OBC can identify USS response frames. Transmits exactly AIRMAC_SIZE
 *         bytes with a 100 ms timeout, then calls ClearTxBuffer().
 *********************************************************************************/
void TransmitBufferUART(void);


/********************************************************************************
 * @brief  Transmits tx_buffer over UART1 on the RS-485 bus.
 *
 * @note   Driver Enable (DE) and Reset Enable (RE) GPIO toggling for the RS-485
 *         transceiver is pending hardware bring-up. Does not clear tx_buffer
 *         after sending.
 *********************************************************************************/
void TransmitBufferRS485(void);


/********************************************************************************
 * @brief  Pulses the LS-02 RS-485 reset line to re-enable the module.
 *
 * @note   Drives PC9 high for 10 ms via HAL_Delay(), then pulls it low.
 *         Call this after completing a command exchange to restore LS-02
 *         to its listening state.
 *********************************************************************************/
void ResetLS02(void);


/********************************************************************************
 * @brief  Parses rx_buffer to extract the instruction number and opcode, and
 *         validates the frame length against the command's opcode requirement.
 *
 * @note   Reads rx_buffer[1] as the instruction number and, if the command
 *         takes an opcode, copies rx_buffer[2..6] into instr_opcode. Rejects
 *         frames whose size does not match the expected length (2 bytes for
 *         no-opcode commands, OPCODE_SIZE+2 bytes for opcode commands), writing
 *         COMMAND_INCORRECT_PARAMETER_FAILURE into tx_buffer[1].
 *
 * @return CMD_OK if the frame is valid and the command was found.
 *         CMD_NOT_FOUND if the instruction number is not in command_table.
 *         CMD_INCORRECT_PARS if the frame length does not match expectations.
 *********************************************************************************/
CMD_ReturnStatus LoadInstructionBuffer(void);


/********************************************************************************
 * @brief  Top-level handler for frames received on the RS-485 bus.
 *
 * @note   Clears rx_flag on entry. Checks rx_buffer[0] against USS_ID and
 *         current state: if the frame is addressed to USS and the board is not
 *         in STATE_IGNORE, calls LoadInstructionBuffer() and transitions to
 *         STATE_EXECUTE_COMMAND on success or STATE_TRANSMIT_RESPONSE on
 *         error. Frames not addressed to USS transition state to fallback_state.
 *         Any pending delayed photo (delayed_flag == 1) is cancelled on a valid
 *         USS frame.
 *
 * @param  fallback_state   State to enter when the received frame is not
 *                          addressed to USS (e.g. STATE_IGNORE or
 *                          STATE_DELAYED_PICTURE).
 *********************************************************************************/
void HandleIncomingCommand(app_state_t fallback_state);


#endif
