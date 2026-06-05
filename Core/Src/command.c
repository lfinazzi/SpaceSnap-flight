#include "command.h"
#include "photo.h"
#include "status.h"
#include "sram.h"
#include "comms.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "stm32f2xx_hal.h"

extern IWDG_HandleTypeDef hiwdg;

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
	else if (st == CMD_CAM_BOOT_ERROR) {
		tx_buffer[1] = COMMAND_CAM_BOOT_ERROR;
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
	char log_buf[64];
	// TODO: Opcode parsing

	// TODO: For now, turns CAMB (using it for debug)
	ActivateCAMB();

	HAL_StatusTypeDef ret = CAM_Init(CAM_I2C_ADDR_B);
	if(ret != HAL_OK)
	{
	    sprintf(log_buf, "Camera B init FAILED, ret=%d\r\n", ret);
	    Log(log_buf);
	    Error_Handler();
	}
	Log("Camera B init OK\r\n");

	// Scope probe window — kick IWDG while waiting
	/*for(uint8_t i = 0; i < 60; i++)
	{
	    HAL_IWDG_Refresh(&hiwdg);
	    HAL_Delay(1000);
	}*/


	// TODO: Debug pending
    if (Photo_CaptureRaw(0, board_status.photos_taken, opcode) != HAL_OK) {
        Log("CAMB: photo capture FAILED\r\n");
        DeactivateCAMB();
        return CMD_CAM_DCMI_ERROR;
    }

	HAL_Delay(10);
	DeactivateCAMB();
	//board_status.photos_taken++; 		// Increment the number of photos taken

	// TODO: Check if camera booted correctly. If not, return CMD_CAM_BOOT_ERROR or something

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

CMD_ReturnStatus CMD_DumpPictureFrame(uint8_t *opcode)
{
	DumpRawBuffer(0, L * H * sizeof(uint16_t));		// Takes a long time!
	return CMD_OK;
}
