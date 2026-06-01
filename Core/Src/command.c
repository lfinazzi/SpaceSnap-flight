#include "command.h"
#include "photo.h"
#include "status.h"
#include "sram.h"
#include "comms.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "stm32f2xx_hal.h"


// Command table — add new entries here, matching the extern declaration in command.h
const command_t command_table[] = {
    { "CMD_TakePicture",        CMD_TAKE_PICTURE_ID,          CMD_TakePicture,         HAS_OPCODE},
    { "CMD_TakePictureDelayed", CMD_TAKE_PICTURE_DELAYED_ID,  CMD_TakePictureDelayed,  HAS_OPCODE},
    { "CMD_GetStatus",          CMD_GET_STATUS_ID,            CMD_GetStatus,           NO_OPCODE},
    { "CMD_DumpPictureFrame",   CMD_DUMP_PICTUREFRAME_ID,     CMD_DumpPictureFrame,    HAS_OPCODE}
    // ... add more here
};

const uint16_t COMMAND_COUNT = sizeof(command_table) / sizeof(command_table[0]);

const command_t* current_command_pointer = NULL;
CMD_ReturnStatus cmd_ret;

uint32_t picture_delay_start = 0;				// Moment the delayed photo instruction was executed
uint8_t picture_delay_mins = 0;					// Amount of N-minute intervals to take a delayed photo
uint8_t delayed_flag = 0;						// Flag used to transmit scheduled command buffer for CMD_TakeDelayedPicture() only once

uint8_t ignore_flag = 0;						// Used to only transmit "Waiting for reset" in debug UART the first time you enter STATE_IGNORE


void ReturnCode(CMD_ReturnStatus st)
{
	if (st == CMD_OK)
	{
		char log_buf[128];
		int offset = sprintf(log_buf, "Command executed: %s | opcode: ", current_command_pointer->name);		// Log to tell with which opcode command was executed with
		for (int i = 0; i < OPCODE_SIZE; i++)
		{
			offset += sprintf(log_buf + offset, "%02X ", instr_opcode[i]);
		}
		sprintf(log_buf + offset, "\r\n");
		Log(log_buf);
		tx_buffer[1] = COMMAND_SUCCESS;		// Command executing successfully
	}
	else if (st == CMD_ERROR) {
	  tx_buffer[1] = COMMAND_ERROR;			// Command return generic error!
	}
	else if (st == CMD_BUFFER_UNOCCUPIED) {
		  tx_buffer[1] = COMMAND_BUFFER_UNOCCUPIED;			// Command return generic error!
	}
	else if (st == CMD_BUFFER_OOB) {
		tx_buffer[1] = COMMAND_BUFFER_OUT_OF_BOUNDS;
	}
	// TODO: when implementing Commands, different runtime errors will be handled here! Add accordingly.
}


// ===== Lookup Function =====
const command_t* GetCommand(uint8_t instruction_number)
{
    for (size_t i = 0; i < COMMAND_COUNT; i++) {
        if (command_table[i].instruction_number == instruction_number) {
            return &command_table[i];
        }
    }
    Log("Invalid command received, not executed\r\n");
    return NULL;
}

// ===== Execute Function =====
CMD_ReturnStatus ExecuteCommand(const command_t *command, uint8_t *opcode)
{
    CMD_ReturnStatus st = CMD_OK;
    if (!command) {
        tx_buffer[1] = COMMAND_NOT_FOUND_FAILURE;
        board_status.last_cmd_status = CMD_NOT_FOUND;
        return CMD_NOT_FOUND;
    }

    st = command->handler(opcode);
    ReturnCode(st);																	// writes the correct return code to tx_buffer[1]

    if (command->instruction_number != CMD_GET_STATUS_ID){
    	board_status.last_instruction = instr_number;
    	board_status.last_cmd_status = st;											// Logs last command return executed
    	memcpy(board_status.last_opcode, instr_opcode, sizeof(instr_opcode));		// Logs last command opcode
    }
    return st;
}

CMD_ReturnStatus CMD_TakePicture(uint8_t *opcode)
{
  // TODO: Change when boards arrive. Implement opcode to select buffer
  //for (uint32_t i = 0; i < L * H; i++)
  //    raw_buffer_1->data[i] = (uint16_t)(i & 0xFFFF);
  //raw_buffer_1->timestamp = HAL_GetTick();
  //raw_buffer_1->designator = board_status.photos_taken;
  //board_status.photos_taken++;
  //board_status.raw_buffer_1_occupied = 1;
  return CMD_OK;
}

// CRC handled by Endurosat OBC
CMD_ReturnStatus CMD_TakePictureDelayed(uint8_t *opcode) {
	picture_delay_mins = opcode[4]; 							// delay is Byte 5 of opcode
	picture_delay_start = HAL_GetTick();

	// write tx_buffer for USS return
	tx_buffer[1] = COMMAND_SCHEDULED;
	tx_buffer[2] = picture_delay_mins;

	char log_buf[64];
    sprintf(log_buf, "Scheduling delayed photo: %u x %umin intervals\r\n", picture_delay_mins, MIN_INTERVAL);
    Log(log_buf);

	return CMD_SCHEDULED;
}

CMD_ReturnStatus CMD_GetStatus(uint8_t *opcode)
{
    board_status.uptime_ms = HAL_GetTick();
    _Static_assert(sizeof(board_status_t) <= AIRMAC_SIZE - 1, "board_status_t too large for tx_buffer");	// static assert for status_t size

    memcpy(&tx_buffer[1], &board_status, sizeof(board_status_t));		// outputs the board status to the tx_buffer
    return CMD_OK;
}

// MAX chunk: 13FF - TODO: Why doesn't this reach until the end. FIX, there's a bug here
CMD_ReturnStatus CMD_DumpPictureFrame(uint8_t *opcode)
{
	uint8_t buffer_choice = opcode[0];
	const uint32_t CHUNK_SIZE  = AIRMAC_SIZE - 2U;								// 117 bytes fit in tx_buffer[2..118].
	const uint32_t total_bytes = (uint32_t)L * H * sizeof(uint16_t);			// 614400 — full frame size
	const uint16_t chunk_index = ((uint16_t)opcode[1] << 8) | opcode[2];		// ME: opcode[1]=MSB, opcode[2]=LSB
	const uint32_t byte_offset = (uint32_t)chunk_index * CHUNK_SIZE;

	volatile raw_photo_t *buf = NULL;

	if (buffer_choice == 1 && board_status.raw_buffer_1_occupied) {
		buf = raw_buffer_1;
	} else if (buffer_choice == 2 && board_status.raw_buffer_2_occupied) {
		buf = raw_buffer_2;
	} else if (buffer_choice == 3 && board_status.raw_buffer_3_occupied) {
		buf = raw_buffer_3;
	} else {
		Log("DumpPicture: invalid buffer or buffer not occupied\r\n");
		return CMD_BUFFER_UNOCCUPIED;
	}

	char header[80];
	sprintf(header, "--- Buffer %u | designator=%u | ts=%lu ---\r\n",
	        buffer_choice, buf->designator, buf->timestamp);
	Log(header);

	if (byte_offset >= total_bytes) {
		Log("DumpPicture: chunk index out of range\r\n");
		return CMD_BUFFER_OOB;
	}

	volatile uint8_t *src    = (volatile uint8_t *)buf->data + byte_offset;
	uint32_t          actual = ((byte_offset + CHUNK_SIZE) <= total_bytes) ? CHUNK_SIZE : (total_bytes - byte_offset);

	// Fill tx_buffer[2..] — tx_buffer[0] and [1] are set by TransmitBufferUART and ExecuteCommand
	for (uint32_t i = 0; i < actual; i++)
		tx_buffer[2 + i] = (uint8_t) src[i];

	// Mirror to debug UART in 32-byte lines
	sprintf(header, "Buf%u chunk%u off=%lu len=%lu\r\n",
	        buffer_choice, chunk_index, byte_offset, actual);
	Log(header);

	char line[32 * 3 + 3];
	for (uint32_t i = 0; i < actual; i += 32) {
		uint32_t chunk  = ((i + 32U) <= actual) ? 32U : (actual - i);
		int      offset = 0;
		for (uint32_t j = 0; j < chunk; j++)
			offset += sprintf(line + offset, "%02X ", (unsigned int) tx_buffer[2 + i + j]);
		sprintf(line + offset, "\r\n");
		Log(line);
	}

	return CMD_OK;
}
